#include "yrc-common.h"
#include "llist.h"
#include "tokenizer.h"
#include <stdlib.h> /* malloc + free */
#include <stdio.h>

#define DEBUG_VISIT(a) //printf("%s\n", #a);

struct yrc_op_s {
  int c;
  struct yrc_op_s* next[4];
};

const char* STATE_MAP[] = {
  "YRC_TKS_NULL",
  "ERROR",
  "DONE"
#define XX(a, b) , #a
  YRC_TOKENIZER_STATE_MAP(XX)
#undef XX
};

const char* TOKEN_TYPES_MAP[] = {
  "EOF"
#define XX(a, b) , #b 
  YRC_TOKEN_TYPES(XX)
#undef XX
};

const char* TOKEN_OPERATOR_MAP[] = {
  "NULL_OP"
#define XX(a, b) , a
  YRC_OPERATOR_MAP(XX)
#undef XX
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
  EQ_EQEQ_NUL = {'=', {&EQ_EQ_NUL, &OP_TERM, 0}},
  BANG_EQEQ_NUL = {'!', {&OP_TERM, &EQ_EQ_NUL, 0}},
  GT_EQ_NUL = {'>', {&EQ_NUL, &OP_TERM, 0}},
  GT_GT_EQ_NUL = {'>', {&GT_EQ_NUL, &OP_TERM, &EQ_NUL, 0}},
  GT_GTGT_EQ_NUL = {'>', {&GT_GT_EQ_NUL, &OP_TERM, &EQ_NUL, 0}},
  LT_EQ_NUL = {'<', {&OP_TERM, &EQ_NUL, 0}},
  LT_LT_EQ_NUL = {'<', {&LT_EQ_NUL, &EQ_NUL, &OP_TERM, 0}};

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

static yrc_token_t* _add_token(yrc_tokenizer_t* state, yrc_token_type type) {
  yrc_token_t* tk;

  tk = malloc(sizeof(*tk));
  if (tk == NULL) {
    return NULL;
  }
  state->state = YRC_TKS_DEFAULT;
  tk->type = type;
  tk->line = state->line;
  tk->fpos = state->fpos;
  tk->col = state->col;
  if (yrc_llist_push(state->tokens, tk)) {
    free(tk);
    return NULL;
  }
  return tk;
}


static yrc_token_operator_t _string_to_operator(char* data, size_t size) {
  DEBUG_VISIT(_string_to_operator)
  size_t i;
  size_t j;

  for(i = 1; i < sizeof(TOKEN_OPERATOR_MAP) / sizeof(TOKEN_OPERATOR_MAP[0]); ++i) {
    for(j = 0; j < size && TOKEN_OPERATOR_MAP[i][j] != 0; ++j) {
      if (TOKEN_OPERATOR_MAP[i][j] != data[j]) {
        break;
      }
    }

    if (j == size && TOKEN_OPERATOR_MAP[i][j] == 0) {
      return i;
    }
  }
  return 0;
}


static void _take(yrc_tokenizer_t* state, char c) {
  ++state->fpos;
  if (c == '\n') {
    ++state->line;
    state->col = 0;
  } else {
    ++state->col;
  }
}


int yrc_tokenizer_init(yrc_tokenizer_t** state) {
  yrc_tokenizer_t* obj = malloc(sizeof(*obj));
  if (obj == NULL) {
    return 1;
  }
  obj->state = YRC_TKS_DEFAULT;
  obj->comment_delim = YRC_COMMENT_DELIM_NONE;
  obj->string_delim = YRC_STRING_DELIM_NONE;
  obj->last_char = 0;

  if (yrc_llist_init(&obj->tokens)) {
    return 1;
  }

  if (yrc_accum_init(&obj->accum_primary, 512)) {
    yrc_llist_free(obj->tokens);
    free(obj);
    return 1;
  }

  if (yrc_accum_init(&obj->accum_secondary, 32)) {
    yrc_llist_free(obj->tokens);
    yrc_accum_free(obj->accum_primary);
    free(obj);
    return 1;
  }

  obj->op_last =
  obj->op_current = NULL;

  *state = obj;
  return 0;
}


int yrc_tokenizer_free(yrc_tokenizer_t* state) {
  yrc_accum_free(state->accum_secondary);
  yrc_accum_free(state->accum_primary);
  free(state);
  return 0;
}


int _yrc_run_default(yrc_tokenizer_t* state, char* data, size_t size, size_t* offs) {
  DEBUG_VISIT(_yrc_run_default)

  yrc_token_t* tk;
  char peek;
  peek = data[*offs];
  switch (peek) {
    case '\0':
      state->state = YRC_TKS_DONE;
      return 0;
    case '"':
      state->state = YRC_TKS_STRING;
      state->string_delim = YRC_STRING_DELIM_DOUBLE;
      ++*offs;
      ++state->fpos;
      ++state->col;
      return 0;
    case '\'':
      state->state = YRC_TKS_STRING;
      state->string_delim = YRC_STRING_DELIM_SINGLE;
      ++*offs;
      ++state->fpos;
      ++state->col;
      return 0;
#define XX(a) case a:
    WHITESPACE_MAP(XX)
      state->state = YRC_TKS_WHITESPACE;
      return 0;
    FASTOP_MAP(XX)
      tk = _add_token(state, YRC_TOKEN_OPERATOR);
      if (tk == NULL) {
        return 1;
      }
      tk->info.as_operator = _string_to_operator(data + *offs, 1);
      ++*offs;
      ++state->fpos;
      ++state->col;
      break;
    NUMERIC_MAP(XX)
      state->last_char = 0;
      state->seen_flags = 0;
      state->state = YRC_TKS_NUMBER;
      break;
    OPERATOR_MAP(XX)
      state->op_last =
      state->op_current = &OP_ROOT;
      state->state = YRC_TKS_OPERATOR;
      break;
    ALPHA_MAP(XX)
      state->state = YRC_TKS_IDENTIFIER;
      break;
#undef XX
  }

  return 0;
}


int _yrc_run_string(yrc_tokenizer_t* state, char* data, size_t size, size_t* offs) {
  DEBUG_VISIT(_yrc_run_string)

  char delim = state->string_delim == YRC_STRING_DELIM_DOUBLE ? '"' : '\'';
  yrc_tokenizer_state st = state->state;
  size_t offset = *offs;
  size_t diff;
  size_t tokensize;
  char* tokendata;
  yrc_token_t* tk;

  while (offset < size) {
    if (data[offset] == '\n' || data[offset] == '\0') {
      return 1;
    }

    if (data[offset] == delim) {
      st = YRC_TKS_DEFAULT;
      break;
    }

    if (data[offset] == '\\') {
      st = YRC_TKS_STRING_ESCAPE;
      break;
    }

    ++offset;
  }

  state->state = st;
  diff = offset - *offs;
  state->fpos += diff;
  state->col += diff;
  if (yrc_accum_copy(state->accum_primary, data + *offs, diff)) {
    return 1;
  }
  *offs = offset;
  if (offset == size) {
    return 0;
  }
  ++*offs;
  ++state->fpos;
  ++state->col;
  if (st == YRC_TKS_STRING_ESCAPE) {
    return 0;
  }
  if (yrc_accum_export(state->accum_primary, &tokendata, &tokensize)) {
    return 1;
  }
  tk = _add_token(state, YRC_TOKEN_STRING);
  if (tk == NULL) {
    return 1;
  }
  tk->info.as_string.delim = state->string_delim;
  tk->info.as_string.data = tokendata;
  tk->info.as_string.size = tokensize;
  return 0;
}


int _yrc_run_whitespace(yrc_tokenizer_t* state, char* data, size_t size, size_t* offs) {
  DEBUG_VISIT(_yrc_run_whitespace)

  size_t tokensize;
  size_t offset;
  size_t diff;
  char* tokendata;
  yrc_token_t* tk;

  offset = *offs;
#define XX(a) data[offset] == a || 
  while (offset < size && (WHITESPACE_MAP(XX) 0)) {
#undef XX
    _take(state, data[offset]);
    ++offset;
  }

  diff = offset - *offs;
  if (yrc_accum_copy(state->accum_primary, data + *offs, diff)) {
    return 1;
  }
  *offs = offset;

  if (offset == size) {
    return 0;
  }

  if (yrc_accum_export(state->accum_primary, &tokendata, &tokensize)) {
    return 1;
  }

  tk = _add_token(state, YRC_TOKEN_WHITESPACE);
  if (tk == NULL) {
    return 1;
  }
  tk->info.as_whitespace.data = tokendata;
  tk->info.as_whitespace.size = tokensize;
  return 0;
}


int _yrc_run_string_escape(yrc_tokenizer_t* state, char* data, size_t size, size_t* offs) {
  DEBUG_VISIT(_yrc_run_string_escape)

  switch (data[*offs]) {
    case 'u':
    case 'U':
      state->state = YRC_TKS_STRING_UNICODE;
      break;
    case 'x':
    case 'X':
      state->state = YRC_TKS_STRING_HEX;
    break;
    case '\\':
      state->state = YRC_TKS_STRING;
      yrc_accum_push(state->accum_primary, '\\');
      break;
    case '"':
      state->state = YRC_TKS_STRING;
      yrc_accum_push(state->accum_primary, '"');
      break;
    case '\'':
      state->state = YRC_TKS_STRING;
      yrc_accum_push(state->accum_primary, '\'');
      break;
    case 'n':
      state->state = YRC_TKS_STRING;
      yrc_accum_push(state->accum_primary, '\n');
      break;
    case 't':
      state->state = YRC_TKS_STRING;
      yrc_accum_push(state->accum_primary, '\t');
      break;
    case 'r':
      state->state = YRC_TKS_STRING;
      yrc_accum_push(state->accum_primary, '\r');
      break;
    case 'v':
      state->state = YRC_TKS_STRING;
      yrc_accum_push(state->accum_primary, '\v');
      break;
    case 'b':
      state->state = YRC_TKS_STRING;
      yrc_accum_push(state->accum_primary, '\b');
      break;
    case 'f':
      state->state = YRC_TKS_STRING;
      yrc_accum_push(state->accum_primary, '\f');
      break;
    case '0':
      state->state = YRC_TKS_STRING;
      yrc_accum_push(state->accum_primary, '\0');
      break;
    default:
      return 1;
  }
  ++*offs;
  ++state->fpos;
  ++state->col;
  return 0;
}


int _yrc_run_string_unicode(yrc_tokenizer_t* state, char* data, size_t size, size_t* offs) {
  DEBUG_VISIT(_yrc_run_string_unicode)

  size_t offset = *offs;
}


int _yrc_run_string_hex(yrc_tokenizer_t* state, char* data, size_t size, size_t* offs) {
  DEBUG_VISIT(_yrc_run_string_hex)

  size_t offset = *offs;
}

int _yrc_run_number(yrc_tokenizer_t* state, char* data, size_t size, size_t* offs) {
  DEBUG_VISIT(_yrc_run_number)
  size_t tokensize;
  size_t offset;
  size_t diff;
  char* tokendata;
  char last_char = state->last_char;
  yrc_token_t* tk;
  offset = *offs;

  while (offset < size) {
    switch (data[offset]) {
      case '.':
      // decimal!
      if (state->seen_flags & REPR_SEEN_ANY) {
        state->state = YRC_TKS_DEFAULT;
        goto exit;
      }
      state->seen_flags |= REPR_SEEN_DOT;
      break;
      case 'a': case 'A':
      case 'b': case 'B':
      case 'c': case 'C':
      case 'd': case 'D':
      case 'f': case 'F':
      if (state->seen_flags & REPR_SEEN_HEX) {
        break;
      }
      return 1;
      case 'e':
      case 'E':
      if (state->seen_flags & REPR_SEEN_HEX) {
        break;
      }
      if (state->seen_flags & REPR_SEEN_EXP) {
        return 1;
      }
      state->seen_flags |= data[offset] == 'e' ? REPR_SEEN_EXP_LE : REPR_SEEN_EXP_BE;
      break;
      case 'x':
      if (last_char != '0') {
        return 1;
      }
      state->seen_flags |= REPR_SEEN_HEX;
      break;
#define XX(a) case a:
      NUMERIC_MAP(XX)
#undef XX
      break;
      default:
      state->state = YRC_TKS_DEFAULT;
      goto exit;
    }
    last_char = data[offset];
    ++offset;
  }
exit:
  state->last_char = last_char;
  diff = offset - *offs;
  state->fpos += diff;
  state->col += diff;
  if (yrc_accum_copy(state->accum_secondary, data + *offs, diff)) {
    return 1;
  }
  *offs = offset;

  if (offset == size) {
    return 0;
  }

  yrc_accum_push(state->accum_secondary, '\0');
  if (yrc_accum_borrow(state->accum_secondary, &tokendata, &tokensize)) {
    return 1;
  }
  tk = _add_token(state, YRC_TOKEN_NUMBER);
  if (tk == NULL) {
    return 1;
  }
  tk->info.as_number.repr = state->seen_flags;
  if (state->seen_flags & (REPR_SEEN_DOT | REPR_SEEN_EXP)) {
    tk->info.as_number.repr |= REPR_IS_FLOAT;
    tk->info.as_number.data.as_double = strtod(tokendata, NULL);
  } else {
    tk->info.as_number.data.as_int = strtoll(
      state->seen_flags & REPR_SEEN_HEX ? tokendata + 2 : tokendata,
      NULL,
      state->seen_flags & REPR_SEEN_HEX ? 16 : 10
    );
  }
  yrc_accum_discard(state->accum_secondary);

  return 0;
}

int _is_kw(yrc_tokenizer_t* state) {
  return 0;
}

void _advance_op(yrc_tokenizer_t* state, char ch) {
  size_t i;
  size_t j;
  if (state->op_current == NULL) {
    printf("nop\n");
    return;
  }
  if (state->op_current == &OP_ROOT) {
    for (i = 0; i < sizeof OPERATORS; ++i) {
      if (OPERATORS[i]->c == ch) {
        printf("moving from ROOT to %c\n", ch);
        state->op_current = OPERATORS[i];
        return;
      }
    }

    printf("moving from ROOT to NULL\n");
    state->op_current = NULL;
    return;
  }

  for (i = 0; i < 4 && state->op_current->next[i]; ++i) {
    if (state->op_current->next[i]->c == ch) {
      printf("moving from %c to %c\n", state->op_current->c, ch);
      state->op_last = state->op_current;
      state->op_current = state->op_current->next[i];
      return;
    }
  }

  if (state->op_last == &OP_ROOT) {
    state->op_last = state->op_current;
    state->op_current = NULL;
    printf("cannot continue, moving to NULL\n");
    return;
  }

  for(i = 0; i < 4 && state->op_last->next[i]; ++i) {
    if(state->op_last->next[i] == state->op_current) {
      for(j = i + 1; j < 4 && state->op_last->next[j]; ++j) {
        if(state->op_last->next[j]->c == ch) {
          printf("retry: moving from %c to %c\n", state->op_last->c, ch);
          state->op_current = state->op_last->next[j];

          return;
        }
      }

      printf("retry: fail, moving to NULL\n");
      state->op_current = NULL;
      return;
    }
  }

  printf("??? moving to NULL\n");
  state->op_last = state->op_current;
  state->op_current = NULL;
}


int _yrc_run_identifier(yrc_tokenizer_t* state, char* data, size_t size, size_t* offs) {
  DEBUG_VISIT(_yrc_run_identifier)

  size_t tokensize;
  size_t offset;
  size_t diff;
  char* tokendata;
  yrc_token_t* tk;

  offset = *offs;
#define XX(a) data[offset] == a || 
  while (offset < size && (ALPHANUMERIC_MAP(XX) 0)) {
#undef XX
    ++offset;
  }

  diff = offset - *offs;
  state->fpos += diff;
  state->col += diff;
  if (yrc_accum_copy(state->accum_primary, data + *offs, diff)) {
    return 1;
  }
  *offs = offset;

  if (offset == size) {
    return 0;
  }

  if (yrc_accum_export(state->accum_primary, &tokendata, &tokensize)) {
    return 1;
  }

  tk = _add_token(state, _is_kw(state) ? YRC_TOKEN_KEYWORD : YRC_TOKEN_IDENT);
  if (tk == NULL) {
    return 1;
  }
  tk->info.as_ident.data = tokendata;
  tk->info.as_ident.size = tokensize;
  return 0;
}


int _yrc_run_operator(yrc_tokenizer_t* state, char* data, size_t size, size_t* offs) {
  DEBUG_VISIT(_yrc_run_operator)

  size_t tokensize;
  size_t offset;
  size_t diff;
  char* tokendata;
  yrc_token_t* tk;

  offset = *offs;
#define XX(a) data[offset] == a || 
  while (offset < size && (OPERATOR_MAP(XX) 0)) {
#undef XX
    _advance_op(state, data[offset]);
    if (state->op_current == NULL) {
      break;
    }

    ++offset;

    if (state->op_last == &SOLIDUS_EQ_TERM_NUL) {
      if (state->op_current == &STAR_NUL) {
        state->state = YRC_TKS_COMMENT_BLOCK;
        state->last_char = '\0';
        return 0;
      }
      if (state->op_current == &SOLIDUS_NUL) {
        state->state = YRC_TKS_COMMENT_LINE;
        return 0;
      }
    }
    if (state->op_last->next[0] == &OP_TERM ||
        state->op_last->next[1] == &OP_TERM ||
        state->op_last->next[2] == &OP_TERM ||
        state->op_last->next[3] == &OP_TERM) {
      break;
    }
  }

  diff = offset - *offs;
  state->fpos += diff;
  state->col += diff;
  if (yrc_accum_copy(state->accum_secondary, data + *offs, diff)) {
    return 1;
  }
  *offs = offset;
  if (offset == size) {
    return 0;
  }
  if (yrc_accum_borrow(state->accum_secondary, &tokendata, &tokensize)) {
    return 1;
  }
  tk = _add_token(state, YRC_TOKEN_OPERATOR);
  if (tk == NULL) {
    return 1;
  }
  tk->info.as_operator = _string_to_operator(tokendata, tokensize);
  yrc_accum_discard(state->accum_secondary);
  return 0;
}


int _yrc_run_comment_line(yrc_tokenizer_t* state, char* data, size_t size, size_t* offs) {
  DEBUG_VISIT(_yrc_run_comment_line)

  yrc_tokenizer_state st = state->state;
  size_t offset = *offs;
  size_t diff;
  size_t tokensize;
  char* tokendata;
  yrc_token_t* tk;

  while (offset < size) {
    if (data[offset] == '\n' || data[offset] == '\0') {
      st = YRC_TKS_DEFAULT;
      break;
    }
    ++offset;
  }

  state->state = st;
  diff = offset - *offs;
  state->fpos += diff;
  state->col += diff;
  if (yrc_accum_copy(state->accum_primary, data + *offs, diff)) {
    return 1;
  }
  *offs = offset;
  if (offset == size) {
    return 0;
  }
  if (yrc_accum_export(state->accum_primary, &tokendata, &tokensize)) {
    return 1;
  }
  tk = _add_token(state, YRC_TOKEN_COMMENT);
  if (tk == NULL) {
    return 1;
  }
  tk->info.as_comment.delim = 0;
  tk->info.as_comment.data = tokendata;
  tk->info.as_comment.size = tokensize;
  return 0;
}


int _yrc_run_comment_block(yrc_tokenizer_t* state, char* data, size_t size, size_t* offs) {
  DEBUG_VISIT(_yrc_run_comment_block)

  yrc_tokenizer_state st = state->state;
  size_t offset = *offs;
  size_t diff;
  size_t tokensize;
  char* tokendata;
  yrc_token_t* tk;
  char last_char = state->last_char;

  while (offset < size) {
    if (data[offset] == '/' && last_char == '*') {
      st = YRC_TKS_DEFAULT;
      ++offset;
      break;
    }
    if (data[offset] == '\0') {
      return 1;
    }
    last_char = data[offset];
    ++offset;
  }

  state->last_char = last_char;
  state->state = st;
  diff = offset - *offs;
  state->fpos += diff;
  state->col += diff;
  if (yrc_accum_copy(state->accum_primary, data + *offs, diff)) {
    return 1;
  }
  *offs = offset;
  if (offset == size) {
    return 0;
  }
  if (yrc_accum_export(state->accum_primary, &tokendata, &tokensize)) {
    return 1;
  }
  tk = _add_token(state, YRC_TOKEN_COMMENT);
  if (tk == NULL) {
    return 1;
  }
  tk->info.as_comment.delim = 1;
  tk->info.as_comment.data = tokendata;
  tk->info.as_comment.size = tokensize;
  return 0;
}


int yrc_tokenizer_advance(yrc_tokenizer_t* state, char* data, size_t size) {
  size_t offs;
  int err;
  offs = 0;

  while (offs < size) {
    switch (state->state) {
      case YRC_TKS_ERROR: return 1;
      case YRC_TKS_DONE: return 0;
#define XX(a, b) \
    case YRC_TKS_##a: \
      err = _yrc_run_##b(state, data, size, &offs);\
      if (err) {\
        state->state = YRC_TKS_ERROR;\
        return -err;\
      }\
      break;
  YRC_TOKENIZER_STATE_MAP(XX)
#undef XX
    }
  }
  return 0;
}


int iter(void* item, size_t idx, void* ctx, int* stop) {
  yrc_token_t* tk = item;
  printf("%llu:%llu %s <<", tk->line, tk->col, TOKEN_TYPES_MAP[tk->type]);
  switch (tk->type) {
    case YRC_TOKEN_COMMENT:
      fwrite(tk->info.as_comment.data, tk->info.as_comment.size, 1, stdout);
    break;
    case YRC_TOKEN_WHITESPACE:
      printf("%lu", tk->info.as_whitespace.size);
    break;
    case YRC_TOKEN_STRING:
      fwrite(tk->info.as_string.data, tk->info.as_string.size, 1, stdout);
    break;
    case YRC_TOKEN_IDENT:
      fwrite(tk->info.as_ident.data, tk->info.as_ident.size, 1, stdout);
    break;
    case YRC_TOKEN_OPERATOR:
      printf("%s", TOKEN_OPERATOR_MAP[tk->info.as_operator]);
    break;
    case YRC_TOKEN_NUMBER:
      if (tk->info.as_number.repr & REPR_IS_FLOAT) {
        printf("float: %lf", tk->info.as_number.data.as_double);
      } else {
        printf("int: %llu", tk->info.as_number.data.as_int);
      }
    break;
  }
  printf(">>\n");
  return 0;
}


int yrc_tokenizer_finish(yrc_tokenizer_t* state) {
  char buf[1] = {0};
  if (yrc_tokenizer_advance(state, buf, 1)) {
    return 1;
  }

  state->state = YRC_TKS_DONE;
  yrc_llist_iter(state->tokens, iter, NULL);
  return 0;
}
