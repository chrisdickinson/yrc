#include "yrc-common.h"
#include "tokenizer.h"
#include "pool.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "str.h"

/* extends yrc_error_t */
typedef struct yrc_tokenizer_error_s {
  YRC_ERROR_BASE;
  yrc_token_t got;
  yrc_token_t expected;
} yrc_parse_error_t;

typedef struct yrc_op_s {
  int c;
  struct yrc_op_s* next[4];
} yrc_op_t;

typedef enum {
  YRC_TKS_DEFAULT,
  YRC_TKS_WHITESPACE,
  YRC_TKS_STRING,
  YRC_TKS_STRING_ESCAPE,
  YRC_TKS_STRING_UNICODE,
  YRC_TKS_STRING_HEX,
  YRC_TKS_NUMBER,
  YRC_TKS_IDENTIFIER,
  YRC_TKS_OPERATOR,
  YRC_TKS_COMMENT_LINE,
  YRC_TKS_COMMENT_BLOCK,
  YRC_TKS_REGEXP_HEAD,
  YRC_TKS_REGEXP_TAIL,
  YRC_TKS_ERROR,
  YRC_TKS_DONE
} yrc_tokenizer_state;

struct yrc_tokenizer_s {
  yrc_pool_t* token_pool;
  yrc_llist_t* tokens;
  size_t fpos;
  size_t line;
  size_t col;
  size_t offset;
  size_t start;
  size_t size;

  uint_fast8_t eof;
  uint_fast8_t flags;
  char* data;
  size_t chunksz;
  void* readctx;
  yrc_str_t current;
};

const char* TOKEN_TYPES_MAP[] = {
#define XX(a, b) #b,
  YRC_TOKEN_TYPES(XX)
#undef XX
  "EOF"
};

const char* TOKEN_OPERATOR_MAP[] = {
  "NULL_OP",
#define XX(a, b) a,
  YRC_OPERATOR_MAP(XX)
  "", /* KWOP_FENCE */
  YRC_KEYWORD_MAP(XX)
#undef XX
  "" /* FINAL*/
};

yrc_op_t
  OP_ROOT = {'\0', {0}},
  OP_TERM = {'\0', {0}},
  EQ_NUL = {'=', {&OP_TERM, 0}},
  PLUS_NUL = {'+', {&OP_TERM, 0}},
  STAR_NUL = {'*', {&OP_TERM, 0}},
  DASH_NUL = {'-', {&OP_TERM, 0}},
  SOLIDUS_NUL = {'/', {&OP_TERM, 0}},
  SOLIDUS_EQ_TERM_NUL = {'/', {&EQ_NUL, &OP_TERM, &SOLIDUS_NUL, &STAR_NUL}},
  STAR_EQ_TERM_NUL = {'*', {&EQ_NUL, &OP_TERM, 0}},
  PLUS_PLUS_NUL = {'+', {&PLUS_NUL, &OP_TERM, 0}},
  PLUS_PLUSPLUS_EQ_TERM_NUL = {'+', {&OP_TERM, &EQ_NUL, &PLUS_PLUS_NUL, 0}},
  DASH_DASH_NUL = {'-', {&DASH_NUL, &OP_TERM, 0}},
  DASH_DASHDASH_EQ_TERM_NUL = {'-', {&OP_TERM, &EQ_NUL, &DASH_DASH_NUL, 0}},
  OP_OR_NUL = {'|', {&OP_TERM, 0}},
  OR_OR_EQ_NUL = {'|', {&OP_OR_NUL, &OP_TERM, &EQ_NUL, 0}},
  AND_NUL = {'&', {&OP_TERM, 0}},
  AND_AND_EQ_NUL = {'&', {&AND_NUL, &OP_TERM, &EQ_NUL, 0}},
  MOD_EQ_NUL = {'%', {&OP_TERM, &EQ_NUL, 0}},
  XOR_EQ_NUL = {'^', {&OP_TERM, &EQ_NUL, 0}},
  EQ_EQ_NUL = {'=', {&EQ_NUL, &OP_TERM, 0}},
  GT_NUL = {'>', {&OP_TERM, 0, 0}},
  EQ_EQEQ_NUL = {'=', {&EQ_EQ_NUL, &OP_TERM, &GT_NUL, 0}},
  BANG_EQEQ_NUL = {'!', {&OP_TERM, &EQ_EQ_NUL, 0}},
  GT_EQ_NUL = {'>', {&EQ_NUL, &OP_TERM, 0}},
  GT_GT_EQ_NUL = {'>', {&GT_EQ_NUL, &OP_TERM, &EQ_NUL, 0}},
  GT_GTGT_EQ_NUL = {'>', {&GT_GT_EQ_NUL, &OP_TERM, &EQ_NUL, 0}},
  LT_EQ_NUL = {'<', {&OP_TERM, &EQ_NUL, 0}},
  LT_LT_EQ_NUL = {'<', {&LT_EQ_NUL, &EQ_NUL, &OP_TERM, 0}};

void _dbg_op(yrc_op_t* op) {
  if (op == &OP_ROOT) printf("OP_ROOT");
  else if (op == &OP_TERM) printf("OP_TERM");
  else if (op == &EQ_NUL) printf("EQ_NUL");
  else if (op == &PLUS_NUL) printf("PLUS_NUL");
  else if (op == &STAR_NUL) printf("STAR_NUL");
  else if (op == &DASH_NUL) printf("DASH_NUL");
  else if (op == &SOLIDUS_NUL) printf("SOLIDUS_NUL");
  else if (op == &SOLIDUS_EQ_TERM_NUL) printf("SOLIDUS_EQ_TERM_NUL");
  else if (op == &STAR_EQ_TERM_NUL) printf("STAR_EQ_TERM_NUL");
  else if (op == &PLUS_PLUS_NUL) printf("PLUS_PLUS_NUL");
  else if (op == &PLUS_PLUSPLUS_EQ_TERM_NUL) printf("PLUS_PLUSPLUS_EQ_TERM_NUL");
  else if (op == &DASH_DASH_NUL) printf("DASH_DASH_NUL");
  else if (op == &DASH_DASHDASH_EQ_TERM_NUL) printf("DASH_DASHDASH_EQ_TERM_NUL");
  else if (op == &OP_OR_NUL) printf("OP_OR_NUL");
  else if (op == &OR_OR_EQ_NUL) printf("OR_OR_EQ_NUL");
  else if (op == &AND_NUL) printf("AND_NUL");
  else if (op == &AND_AND_EQ_NUL) printf("AND_AND_EQ_NUL");
  else if (op == &MOD_EQ_NUL) printf("MOD_EQ_NUL");
  else if (op == &XOR_EQ_NUL) printf("XOR_EQ_NUL");
  else if (op == &EQ_EQ_NUL) printf("EQ_EQ_NUL");
  else if (op == &EQ_EQEQ_NUL) printf("EQ_EQEQ_NUL");
  else if (op == &BANG_EQEQ_NUL) printf("BANG_EQEQ_NUL");
  else if (op == &GT_EQ_NUL) printf("GT_EQ_NUL");
  else if (op == &GT_GT_EQ_NUL) printf("GT_GT_EQ_NUL");
  else if (op == &GT_GTGT_EQ_NUL) printf("GT_GTGT_EQ_NUL");
  else if (op == &LT_EQ_NUL) printf("LT_EQ_NUL");
  else if (op == &LT_LT_EQ_NUL) printf("LT_LT_EQ_NUL");
}

yrc_op_t* OPERATORS[] = {
  &OR_OR_EQ_NUL,
  &AND_AND_EQ_NUL,
  &SOLIDUS_EQ_TERM_NUL,
  &PLUS_PLUSPLUS_EQ_TERM_NUL,
  &DASH_DASHDASH_EQ_TERM_NUL,
  &STAR_EQ_TERM_NUL,
  &MOD_EQ_NUL,
  &XOR_EQ_NUL,
  &BANG_EQEQ_NUL,
  &EQ_EQEQ_NUL,
  &LT_LT_EQ_NUL,
  &GT_GTGT_EQ_NUL,
};

yrc_operator_t _string_to_operator(yrc_str_t* str) {
  yrc_operator_t i;
  yrc_operator_t end;
  size_t size = yrc_str_len(str);
  char* data = yrc_str_ptr(str);

  switch (size) {
    case 1: i = YRC_OP_EQ; end = YRC_OP_EQEQ; break;
    case 2: i = YRC_OP_EQEQ; end = YRC_OP_LSHFEQ; break;
    case 3: i = YRC_OP_LSHFEQ; end = YRC_OP_URSHFEQ; break;
    case 5: i = YRC_OP_URSHFEQ; end = YRC_OP_LAST; break;
    default: return 0;
  }

  for (; i < end; ++i) {
    if (memcmp(data, TOKEN_OPERATOR_MAP[i], size) == 0) {
      return i;
    }
  }
  return 0;
}


yrc_keyword_t _string_to_keyword(yrc_str_t* str) {
  yrc_keyword_t i;
  yrc_keyword_t end;

  size_t size = yrc_str_len(str);
  char* data = yrc_str_ptr(str);

  switch (size) {
    case 2: i = YRC_KW_DO; end = YRC_KW_FOR; break;
    case 3: i = YRC_KW_FOR; end = YRC_KW_VOID; break;
    case 4: i = YRC_KW_VOID; end = YRC_KW_BREAK; break;
    case 5: i = YRC_KW_BREAK; end = YRC_KW_DELETE; break;
    case 6: i = YRC_KW_DELETE; end = YRC_KW_DEFAULT; break;
    case 7: i = YRC_KW_DEFAULT; end = YRC_KW_FUNCTION; break;
    case 8: i = YRC_KW_FUNCTION; end = YRC_KW_INSTANCEOF; break;
    case 10: i = YRC_KW_INSTANCEOF; end = YRC_KW_LAST; break;
    default: return 0;
  }

  for (; i < end; ++i) {
    if (memcmp(data, TOKEN_OPERATOR_MAP[i], size) == 0) {
      return i;
    }
  }
  return 0;
}


int yrc_tokenizer_init(yrc_tokenizer_t** state, size_t chunksz, void* ctx) {
  yrc_tokenizer_t* obj = malloc(sizeof(*obj));
  if (obj == NULL) {
    return 1;
  }

  obj->eof = 0;
  obj->chunksz = chunksz;
  obj->data = malloc(chunksz);
  obj->readctx = ctx;
  if (obj->data == NULL) {
    free(obj);
    return 1;
  }

  yrc_str_init(&obj->current);
  if (yrc_pool_init(&obj->token_pool, sizeof(yrc_token_t))) {
    free(obj->data);
    free(obj);
    return 1;
  }

  if (yrc_llist_init(&obj->tokens)) {
    yrc_pool_free(obj->token_pool);
    free(obj->data);
    free(obj);
    return 1;
  }

  obj->fpos =
  obj->col = 0;
  obj->line = 1;

  obj->offset =
  obj->start =
  obj->size = 0;
  *state = obj;
  return 0;
}


int _free_tokens(void* raw, size_t idx, void* ctx, int* stop) {
  yrc_token_t* token = (yrc_token_t*)raw;
  switch (token->type) {
    case YRC_TOKEN_STRING:
      yrc_str_free(&token->info.as_string.str);
      break;

    case YRC_TOKEN_IDENT:
      yrc_str_free(&token->info.as_ident.str);
      break;

    case YRC_TOKEN_COMMENT:
      yrc_str_free(&token->info.as_comment.str);
      break;

    case YRC_TOKEN_WHITESPACE:
      yrc_str_free(&token->info.as_whitespace.str);
      break;

    default:
      break;

  }
  return 0;
}


int yrc_tokenizer_free(yrc_tokenizer_t* state) {
  free(state->data);
  yrc_llist_foreach(state->tokens, _free_tokens, NULL);
  yrc_llist_free(state->tokens);
  yrc_pool_free(state->token_pool);
  free(state);
  return 0;
}

void _advance_op(yrc_op_t** op_current, yrc_op_t** op_last, char ch) {
  size_t i;
  size_t j;
  if ((*op_current) == NULL) {
    return;
  }
  if ((*op_current) == &OP_ROOT) {
    for (i = 0; i < sizeof OPERATORS; ++i) {
      if (OPERATORS[i]->c == ch) {
        (*op_current) = OPERATORS[i];
        return;
      }
    }

    (*op_current) = NULL;
    return;
  }

  for (i = 0; i < 4 && (*op_current)->next[i]; ++i) {
    if ((*op_current)->next[i]->c == ch) {
      (*op_last) = (*op_current);
      (*op_current) = (*op_current)->next[i];
      return;
    }
  }

  if ((*op_last) == &OP_ROOT) {
    (*op_last) = (*op_current);
    (*op_current) = NULL;
    return;
  }

  for(i = 0; i < 4 && (*op_last)->next[i]; ++i) {
    if((*op_last)->next[i] == (*op_current)) {
      for(j = i + 1; j < 4 && (*op_last)->next[j]; ++j) {
        if((*op_last)->next[j]->c == ch) {
          (*op_current) = (*op_last)->next[j];

          return;
        }
      }

      (*op_current) = NULL;
      return;
    }
  }

  (*op_last) = (*op_current);
  (*op_current) = NULL;
}


#define TO_CASE(a) case a:

static inline int is_ws(char ch) {
  switch(ch) {
    WHITESPACE_MAP(TO_CASE)
      return 1;
  }
  return 0;
}

static inline int is_op(char ch) {
  switch(ch) {
    OPERATOR_MAP(TO_CASE)
      return 1;
  }
  return 0;
}

static inline int is_alnum(char ch) {
  switch(ch) {
    ALPHANUMERIC_MAP(TO_CASE)
      return 1;
  }
  return 0;
}

int yrc_tokenizer_scan(
    yrc_tokenizer_t* tokenizer, 
    yrc_readcb read, 
    yrc_token_t** out, 
    yrc_scan_allow_regexp regexp_mode) {
  size_t last_fpos, last_line, last_col, fpos, line, col;
  yrc_tokenizer_state state = YRC_TKS_DEFAULT;
  size_t offset, start, diff, size;
  yrc_op_t *op_current, *op_last;
  char last = '\0';
  char* data;
  yrc_keyword_t kw;
  char should_break = 0;
  yrc_token_t* tk;
  uint_fast8_t eof;
  uint_fast8_t flags;
  char delim;
  int pending_read = 0;

  start = tokenizer->start;
  data = tokenizer->data;
  last_fpos = fpos = tokenizer->fpos;
  last_line = line = tokenizer->line;
  last_col = col = tokenizer->col;
  offset = tokenizer->offset;
  size = tokenizer->size;
  eof = tokenizer->eof;
  flags = tokenizer->flags;

  switch (regexp_mode) {
    case YRC_IS_REGEXP_EQ:
      if (yrc_str_push(&tokenizer->current, '=')) {
        return 1;
      }
    case YRC_IS_REGEXP:
      state = YRC_TKS_REGEXP_HEAD;
      break;

    default: break;

  }
  while (!eof) {
    if (offset == size) {
      size = read(tokenizer->data, tokenizer->chunksz, tokenizer->readctx);
      offset = 0;
    }

    if (size == 0) {
      eof = 1;
    }

restart:
    do {
      start = offset;
      switch (state) {
        case YRC_TKS_DEFAULT: {
            if (eof) {
              state = YRC_TKS_DONE;
              break;
            }
            switch (data[offset]) {
              case '"':
                state = YRC_TKS_STRING;
                delim = '"';
                ++offset;
                ++fpos;
                ++col;
                goto restart;
              case '\'':
                state = YRC_TKS_STRING;
                delim = '\'';
                ++offset;
                ++fpos;
                ++col;
                goto restart;
              WHITESPACE_MAP(TO_CASE)
                state = YRC_TKS_WHITESPACE;
                goto restart;
              FASTOP_MAP(TO_CASE)
                tk = yrc_pool_attain(tokenizer->token_pool);
                if (tk == NULL) {
                  return 1;
                }
                tk->type = YRC_TOKEN_OPERATOR;
                {
                  yrc_str_t tmp;
                  tmp.interned.flag = 3; /* interned flag set + length 1 */
                  tmp.interned.data[0] = data[offset];
                  tk->info.as_operator = _string_to_operator(&tmp);
                }
                ++offset;
                ++fpos;
                ++col;
                goto export;
              NUMERIC_MAP(TO_CASE)
                last = 0;
                flags = 0;
                state = YRC_TKS_NUMBER;
                break;

              OPERATOR_MAP(TO_CASE)
                op_last = op_current = &OP_ROOT;
                state = YRC_TKS_OPERATOR;
                break;

              ALPHA_MAP(TO_CASE)
                state = YRC_TKS_IDENTIFIER;
                break;

              default:
                ++offset;
                ++fpos;
                ++col;
                break;
            }
          };
          break;

        case YRC_TKS_WHITESPACE: {
            while(offset < size) {
              if (eof || !is_ws(data[offset])) {
                state = YRC_TKS_DEFAULT;
                break;

              }
              if (data[offset] == '\n') {
                ++line;
                col = 0;
              } else {
                ++col;
              }
              last = data[offset];
              ++offset;
            }
            diff = offset - start;
            fpos += diff;
            if (yrc_str_pushv(&tokenizer->current, data + start, diff)) {
              return 1;
            }
            start = offset;
            if (offset == size) {
              break;

            }
            tk = yrc_pool_attain(tokenizer->token_pool);
            if (tk == NULL) {
              return 1;
            }
            tk->type = YRC_TOKEN_WHITESPACE;
            yrc_str_xfer(&tokenizer->current, &tk->info.as_whitespace.str);
            tk->info.as_whitespace.has_newline = line != last_line;
            goto export;
          };
          break;

        case YRC_TKS_STRING: {
            while(1) {
              if (offset == size && !eof) {
                pending_read = 1;
                break;

              } {
                if (eof || data[offset] == '\n') {
                  return 1;
                }
                if (data[offset] == delim) {
                  state = YRC_TKS_DEFAULT;
                  break;

                }
                if (data[offset] == '\\') {
                  state = YRC_TKS_STRING_ESCAPE;
                  if (yrc_str_pushv(&tokenizer->current, data + start, offset - start)) {
                    return 1;
                  }
                  ++col;
                  ++fpos;
                  ++offset;
                  goto restart;
                }
              }
              last = data[offset];
              ++offset;
            }
            diff = offset - start;
            fpos += diff;
            col += diff;
            if (yrc_str_pushv(&tokenizer->current, data + start, diff)) {
              return 1;
            }
            start = offset;
            if (pending_read) {
              pending_read = 0;
              break;

            }
            tk = yrc_pool_attain(tokenizer->token_pool);
            if (tk == NULL) {
              return 1;
            }
            tk->type = YRC_TOKEN_STRING;
            ++col;
            ++fpos;
            ++offset;
            tk->info.as_string.delim = delim == '\'' ? YRC_STRING_DELIM_SINGLE : YRC_STRING_DELIM_DOUBLE;
            yrc_str_xfer(&tokenizer->current, &tk->info.as_string.str);
            goto export;
          };
          break;

        case YRC_TKS_STRING_ESCAPE: {
            ++col;
            ++fpos;
            ++offset;
            switch (data[offset - 1]) {
              case 'u':
              case 'U':
                state = YRC_TKS_STRING_UNICODE;
                flags = 0;
                goto restart;
              case 'x':
              case 'X':
                state = YRC_TKS_STRING_HEX;
                flags = 0;
                goto restart;
              default:
                return 1;
              case '\\':
                state = YRC_TKS_STRING;
                yrc_str_push(&tokenizer->current, '\\');
                break;

              case '"':
                state = YRC_TKS_STRING;
                yrc_str_push(&tokenizer->current, '"');
                break;

              case '\'':
                state = YRC_TKS_STRING;
                yrc_str_push(&tokenizer->current, '\'');
                break;

              case 'n':
                state = YRC_TKS_STRING;
                yrc_str_push(&tokenizer->current, '\n');
                break;

              case 't':
                state = YRC_TKS_STRING;
                yrc_str_push(&tokenizer->current, '\t');
                break;

              case 'r':
                state = YRC_TKS_STRING;
                yrc_str_push(&tokenizer->current, '\r');
                break;

              case 'v':
                state = YRC_TKS_STRING;
                yrc_str_push(&tokenizer->current, '\v');
                break;

              case 'b':
                state = YRC_TKS_STRING;
                yrc_str_push(&tokenizer->current, '\b');
                break;

              case 'f':
                state = YRC_TKS_STRING;
                yrc_str_push(&tokenizer->current, '\f');
                break;

              case '0':
                state = YRC_TKS_STRING;
                yrc_str_push(&tokenizer->current, '\0');
                break;

            }
          };
          break;

        case YRC_TKS_STRING_UNICODE: {
            while(1) {
              if (eof) return 1;
              if (flags) {
                state = YRC_TKS_STRING;
                flags = 0;
                break;

              }
              switch (data[offset]) {
                case 'a':
                case 'A':
                case 'b':
                case 'B':
                case 'c':
                case 'C':
                case 'd':
                case 'D':
                case 'f':
                case 'F':
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                  break;

                default:
                  return 1;
              } ++flags;
              last = data[offset];
              ++offset;
            }
            diff = offset - start;
            fpos += diff;
            col += diff;
            if (yrc_str_pushv(&tokenizer->current, data + start, diff)) {
              return 1;
            }
            start = offset;
            if (pending_read) {
              pending_read = 0;
              break;

            }
            state = YRC_TKS_STRING;
          };
          break;

        case YRC_TKS_STRING_HEX:
          UNREACHABLE();
          break;

        case YRC_TKS_NUMBER: {
            while(1) {
              if (offset == size && !eof) {
                pending_read = 1;
                break;

              } {
                should_break = 0;
                switch (data[offset]) {
                  case '.':
                    if (flags & REPR_SEEN_ANY) {
                      state = YRC_TKS_DEFAULT;
                      should_break = 1;
                      break;

                    }
                    flags |= REPR_SEEN_DOT;
                    break;

                  case 'a':
                  case 'A':
                  case 'b':
                  case 'B':
                  case 'c':
                  case 'C':
                  case 'd':
                  case 'D':
                  case 'f':
                  case 'F':
                    if (flags & REPR_SEEN_HEX) {
                      break;

                    }
                    return 1;
                  case 'e':
                  case 'E':
                    if (flags & REPR_SEEN_HEX) {
                      break;

                    }
                    if (flags & REPR_SEEN_EXP) {
                      return 1;
                    }
                    flags |= data[offset] == 'e' ? REPR_SEEN_EXP_LE : REPR_SEEN_EXP_BE;
                    break;

                  case 'x':
                    if (last != '0') {
                      return 1;
                    }
                    flags |= REPR_SEEN_HEX;
                    break;

                  case '0':
                  case '1':
                  case '2':
                  case '3':
                  case '4':
                  case '5':
                  case '6':
                  case '7':
                  case '8':
                  case '9':
                    break;

                  default:
                    state = YRC_TKS_DEFAULT;
                    should_break = 1;
                    break;

                }
                if (should_break) {
                  should_break = 0;
                  break;

                }
              }
              last = data[offset];
              ++offset;
            }
            diff = offset - start;
            fpos += diff;
            col += diff;
            if (yrc_str_pushv(&tokenizer->current, data + start, diff)) {
              return 1;
            }
            start = offset;
            if (pending_read) {
              pending_read = 0;
              break;

            }
            tk = yrc_pool_attain(tokenizer->token_pool);
            if (tk == NULL) {
              return 1;
            }
            tk->type = YRC_TOKEN_NUMBER;
            tk->info.as_number.repr = flags;
            if (flags & (REPR_SEEN_DOT | REPR_SEEN_EXP)) {
              tk->info.as_number.repr |= REPR_IS_FLOAT;
              tk->info.as_number.data.as_double = strtod(yrc_str_ptr(&tokenizer->current), NULL);
            } else {
              tk->info.as_number.data.as_int = strtoll(yrc_str_ptr(&tokenizer->current) + (flags & REPR_SEEN_HEX ? 2 : 0), NULL, flags & REPR_SEEN_HEX ? 16 : 10 );
            }
            if (yrc_str_xfer(&tokenizer->current, NULL)) {
              return 1;
            }
            goto export;
          };
          break;

        case YRC_TKS_IDENTIFIER: {
            while(1) {
              if (offset == size && !eof) {
                pending_read = 1;
                break;

              } {
                if (eof || !is_alnum(data[offset])) {
                  state = YRC_TKS_DEFAULT;
                  break;

                }
              }
              last = data[offset];
              ++offset;
            }
            diff = offset - start;
            fpos += diff;
            col += diff;
            if (yrc_str_pushv(&tokenizer->current, data + start, diff)) {
              return 1;
            }
            start = offset;
            if (pending_read) {
              pending_read = 0;
              break;

            }
            tk = yrc_pool_attain(tokenizer->token_pool);
            if (tk == NULL) {
              return 1;
            }
            tk->type = YRC_TOKEN_IDENT;
            kw = _string_to_keyword(&tokenizer->current);
            if (kw != 0) {
              tk->type = YRC_TOKEN_KEYWORD;
              tk->info.as_keyword = kw;
              yrc_str_xfer(&tokenizer->current, NULL);
            } else {
              yrc_str_xfer(&tokenizer->current, &tk->info.as_ident.str);
            }
            goto export;
          };
          break;

        case YRC_TKS_OPERATOR: {
            while(1) {
              if (offset == size && !eof) {
                pending_read = 1;
                break;

              } {
                if (!is_op(data[offset])) {
                  state = YRC_TKS_DEFAULT;
                  break;

                }
                _advance_op(&op_current, &op_last, data[offset]);
                if (op_current == NULL) {
                  state = YRC_TKS_DEFAULT;
                  break;

                }
                if (op_last == &SOLIDUS_EQ_TERM_NUL) {
                  if (op_current == &STAR_NUL) {
                    state = YRC_TKS_COMMENT_BLOCK;
                    last = '\0';
                    yrc_str_xfer(&tokenizer->current, NULL);
                    ++offset;
                    ++fpos;
                    ++col;
                    goto restart;
                  }
                  if (op_current == &SOLIDUS_NUL) {
                    state = YRC_TKS_COMMENT_LINE;
                    yrc_str_xfer(&tokenizer->current, NULL);
                    ++offset;
                    ++fpos;
                    ++col;
                    goto restart;
                  }
                }
                if (!op_current && (op_last->next[0] == &OP_TERM || op_last->next[1] == &OP_TERM || op_last->next[2] == &OP_TERM || op_last->next[3] == &OP_TERM)) {
                  state = YRC_TKS_DEFAULT;
                  ++offset;
                  break;

                }
              }
              last = data[offset];
              ++offset;
            }
            diff = offset - start;
            fpos += diff;
            col += diff;
            if (yrc_str_pushv(&tokenizer->current, data + start, diff)) {
              return 1;
            }
            start = offset;
            if (pending_read) {
              pending_read = 0;
              break;

            }
            tk = yrc_pool_attain(tokenizer->token_pool);
            if (tk == NULL) {
              return 1;
            }
            tk->type = YRC_TOKEN_OPERATOR;
            tk->info.as_operator = _string_to_operator(&tokenizer->current);
            if (yrc_str_xfer(&tokenizer->current, NULL)) {
              return 1;
            }
            goto export;
          };
          break;

        case YRC_TKS_COMMENT_LINE: {
            while(1) {
              if (offset == size && !eof) {
                pending_read = 1;
                break;

              } {
                if (eof || data[offset] == '\n') {
                  state = YRC_TKS_DEFAULT;
                  break;

                }
              }
              last = data[offset];
              ++offset;
            }
            diff = offset - start;
            fpos += diff;
            col += diff;
            if (yrc_str_pushv(&tokenizer->current, data + start, diff)) {
              return 1;
            }
            start = offset;
            if (pending_read) {
              pending_read = 0;
              break;

            }
            tk = yrc_pool_attain(tokenizer->token_pool);
            if (tk == NULL) {
              return 1;
            }
            tk->type = YRC_TOKEN_COMMENT;
            tk->info.as_comment.delim = 0;
            yrc_str_xfer(&tokenizer->current, &tk->info.as_comment.str);
            goto export;
          };
          break;

        case YRC_TKS_COMMENT_BLOCK: {
            while(1) {
              if (offset == size && !eof) {
                pending_read = 1;
                break;

              } {
                if (eof) return 1;
                if (data[offset] == '\n') {
                  ++line;
                  col = 0;
                } else {
                  ++col;
                }
                if (data[offset] == '/' && last == '*') {
                  state = YRC_TKS_DEFAULT;
                  ++offset;
                  break;

                }
              }
              last = data[offset];
              ++offset;
            }
            diff = offset - start;
            fpos += diff;
            col += diff;
            if (yrc_str_pushv(&tokenizer->current, data + start, diff)) {
              return 1;
            }
            start = offset;
            if (pending_read) {
              pending_read = 0;
              break;

            }
            tk = yrc_pool_attain(tokenizer->token_pool);
            if (tk == NULL) {
              return 1;
            }
            tk->type = YRC_TOKEN_COMMENT;
            tk->info.as_comment.delim = 1;
            yrc_str_xfer(&tokenizer->current, &tk->info.as_comment.str);
            goto export;
          };
          break;

        case YRC_TKS_REGEXP_HEAD: {
            while(1) {
              if (offset == size && !eof) {
                pending_read = 1;
                break;

              }
              if (eof || data[offset] == '\n') {
                return 1;
              }
              if (data[offset] == '/' && last != '\\') {
                state = YRC_TKS_REGEXP_TAIL;
                break;

              }
              if (data[offset] == '\\' && last == '\\') {
                last = '\0';
              } else {
                last = data[offset];
              } ++offset;
            }
            diff = offset - start;
            fpos += diff;
            col += diff;
            if (yrc_str_pushv(&tokenizer->current, data + start, diff)) {
              return 1;
            }
            start = offset;
            if (pending_read) {
              pending_read = 0;
              break;

            } ++offset;
            flags = 0;
          };
          break;

        case YRC_TKS_REGEXP_TAIL: {
            while(1) {
              if (offset == size && !eof) {
                pending_read = 1;
                break;

              }
              if (eof) {
                state = YRC_TKS_DEFAULT;
                break;

              }
              should_break = 0;
              switch(data[offset]) {
                case 'm':
                  should_break = !((flags ^= YRC_REGEXP_MULTILINE) & YRC_REGEXP_MULTILINE);
                  break;

                case 'y':
                  should_break = !((flags ^= YRC_REGEXP_STICKY) & YRC_REGEXP_STICKY);
                  break;

                case 'g':
                  should_break = !((flags ^= YRC_REGEXP_GLOBAL) & YRC_REGEXP_GLOBAL);
                  break;

                case 'i':
                  should_break = !((flags ^= YRC_REGEXP_IGNORECASE) & YRC_REGEXP_IGNORECASE);
                  break;

                default:
                  should_break = 1;
                  break;

              }
              if (should_break) {
                should_break = 0;
                break;

              }
              last = data[offset];
              ++offset;
            }
            diff = offset - start;
            fpos += diff;
            col += diff;
            start = offset;
            if (pending_read) {
              pending_read = 0;
              break;

            }
            tk = yrc_pool_attain(tokenizer->token_pool);
            if (tk == NULL) {
              return 1;
            }
            tk->type = YRC_TOKEN_REGEXP;
            yrc_str_xfer(&tokenizer->current, &tk->info.as_regexp.str);
            tk->info.as_regexp.flags = flags;
            goto export;
          };
          break;




        default:
          printf("in weird state %d", state);
          UNREACHABLE();
          break;

      }
    } while (offset < size);


  }
  *out = NULL;
  tokenizer->start = start; tokenizer->fpos = fpos; tokenizer->line = line; tokenizer->col = col; tokenizer->offset = offset; tokenizer->size = size; tokenizer->flags = flags; tokenizer->eof = eof;;
  return 0;
export:
  tk->start.fpos = last_fpos;
  tk->start.line = last_line;
  tk->start.col = last_col;
  tk->end.fpos = fpos;
  tk->end.line = line;
  tk->end.col = col;
  if (yrc_llist_push(tokenizer->tokens, tk)) {
    return 1;
  }
  *out = tk;
  tokenizer->start = start; tokenizer->fpos = fpos; tokenizer->line = line; tokenizer->col = col; tokenizer->offset = offset; tokenizer->size = size; tokenizer->flags = flags; tokenizer->eof = eof;;
  return 0;
}


void yrc_token_repr(yrc_token_t* tk) {
  printf("%lu:%lu %s ⟪ ", tk->start.line, tk->start.col, TOKEN_TYPES_MAP[tk->type]);
  switch (tk->type) {
    case YRC_TOKEN_EOF:
      printf("(eof)");
    break;

    case YRC_TOKEN_COMMENT:
      fwrite(yrc_str_ptr(&tk->info.as_comment.str), yrc_str_len(&tk->info.as_comment.str), 1, stdout);
    break;

    case YRC_TOKEN_WHITESPACE:
      printf("%lu", yrc_str_len(&tk->info.as_whitespace.str));
    break;

    case YRC_TOKEN_STRING:
      fwrite(yrc_str_ptr(&tk->info.as_string.str), yrc_str_len(&tk->info.as_string.str), 1, stdout);
    break;

    case YRC_TOKEN_IDENT:
      fwrite(yrc_str_ptr(&tk->info.as_ident.str), yrc_str_len(&tk->info.as_ident.str), 1, stdout);
    break;

    case YRC_TOKEN_OPERATOR:
    case YRC_TOKEN_KEYWORD:
      printf("%s", TOKEN_OPERATOR_MAP[tk->info.as_operator]);
    break;

    case YRC_TOKEN_NUMBER:
      if (tk->info.as_number.repr & REPR_IS_FLOAT) {
        printf("float: %lf", tk->info.as_number.data.as_double);
      } else {
        printf("int: %llu", tk->info.as_number.data.as_int);
      }
    break;

    case YRC_TOKEN_REGEXP:
      printf("/");
      fwrite(yrc_str_ptr(&tk->info.as_regexp.str), yrc_str_len(&tk->info.as_regexp.str), 1, stdout);
      printf("/");
      if (tk->info.as_regexp.flags & YRC_REGEXP_MULTILINE) {
        printf("m");
      }
      if (tk->info.as_regexp.flags & YRC_REGEXP_GLOBAL) {
        printf("g");
      }
      if (tk->info.as_regexp.flags & YRC_REGEXP_STICKY) {
        printf("y");
      }
      if (tk->info.as_regexp.flags & YRC_REGEXP_IGNORECASE) {
        printf("i");
      }
    break;

  }
  printf(" ⟫\n");
}

int yrc_tokenizer_promote_keyword(yrc_tokenizer_t* tokenizer, yrc_token_t* token) {
  const char* target = TOKEN_OPERATOR_MAP[token->info.as_keyword];
  size_t size = strlen(target);
  token->type = YRC_TOKEN_IDENT;
  yrc_str_init(&token->info.as_ident.str);
  if (yrc_str_pushv(&token->info.as_ident.str, target, size)) {
    return 1;
  }
  return 0;
}

int yrc_tokenizer_eof(yrc_tokenizer_t* state) {
  return state->eof;
}


int iter(void* item, size_t idx, void* ctx, int* stop) {
  yrc_token_repr((yrc_token_t*)item);
  return 0;
}
