#include "yrc-common.h"
#include "llist.h"
#include "tokenizer.h"
#include "parser.h"
#include <stdlib.h> /* malloc + free */
#include <stdio.h>

#define IS_EOF(K) (K == &eof)
#define IS_OP(K, T) (K->type == YRC_TOKEN_OPERATOR && K->info.as_operator == YRC_OP_##T)
struct yrc_parser_state_s {
  yrc_token_t* token;
  yrc_parser_symbol_t* symbol;
  yrc_llist_t* tokens;
  int saw_newline;
};


int advance(yrc_parser_state_t*);
int expression(yrc_parser_state_t*, int, yrc_ast_node_t**);
int statement(yrc_parser_state_t*, yrc_ast_node_t**);
int statements(yrc_parser_state_t*, yrc_llist_t*);

int _block(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  yrc_ast_node_block_t* node = malloc(sizeof(*node));
  if (node == NULL) {
    return 1;
  }
  if (yrc_llist_init(&node->body)) {
    free(node);
    return 1;
  }
  if (statements(state, node->body)) {
    yrc_llist_free(node->body);
    free(node);
    return 1;
  }
  *out = (yrc_ast_node_t*)node;
  /* TODO: assert consume `}` */
  return advance(state);
}


int _break(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  yrc_ast_node_break_t* node = malloc(sizeof(*node));
  if (node == NULL) {
    return 1;
  }
  node->kind = YRC_AST_STMT_BREAK;
  node->label = (state->token->type == YRC_TOKEN_IDENT) ?
    node->label = state->token :
    NULL;
  *out = (yrc_ast_node_t*)node;
  return 0;
}


int _call(yrc_parser_state_t* state, yrc_ast_node_t* left, yrc_ast_node_t** out) {
  yrc_ast_node_call_t* node = malloc(sizeof(*node));
  yrc_ast_node_t* item;
  node->callee = left;
  if (yrc_llist_init(&node->arguments)) {
    goto cleanup;
  }
  do {
    item = NULL;
    if (expression(state, 0, &item)) {
      goto cleanupfull;
    }
    if (yrc_llist_push(node->arguments, item)) {
      goto cleanupfull;
    }
  } while(IS_OP(state->token, COMMA));

  /* consume `)` */
  if (advance(state)) {
    goto cleanupfull;
  }
  *out = (yrc_ast_node_t*)node;
  return 0;

cleanupfull:
  yrc_llist_free(node->arguments);
cleanup:
  free(node);
  return 1;
}


int _continue(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  yrc_ast_node_continue_t* node = malloc(sizeof(*node));
  if (node == NULL) {
    return 1;
  }
  node->kind = YRC_AST_STMT_CONTINUE;
  node->label = (state->token->type == YRC_TOKEN_IDENT) ?
    node->label = state->token :
    NULL;
  *out = (yrc_ast_node_t*)node;
  return 0;
}


int _do(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  return 0;
}


int _for(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  return 0;
}


int _if(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  return 0;
}


#define INFIX(NAME, TYPE, RBP_MOD, KIND, EXTRA) \
int NAME(yrc_parser_state_t* state, yrc_ast_node_t* left, yrc_ast_node_t** out) { \
  yrc_parser_symbol_t* sym = state->symbol;\
  yrc_token_t* token = state->token;\
  TYPE* node = malloc(sizeof(*node));\
  yrc_ast_node_binary_t* asbin = (yrc_ast_node_binary_t*)node;\
  printf("infix " #NAME " " #TYPE " " #KIND "\n");\
  sym;token;\
  if (node == NULL) {\
    return 1;\
  }\
  asbin->left = left;\
  if (expression(state, state->symbol->lbp + RBP_MOD, &asbin->right)) {\
    free(asbin->left);\
    free(node);\
    return 1;\
  }\
  asbin->kind = KIND;\
  do { EXTRA } while(0);\
  *out = (yrc_ast_node_t*)node;\
  return 0;\
}

#define PREFIX(NAME, TYPE, BP, KIND, EXTRA)\
int NAME(yrc_parser_state_t* state, yrc_ast_node_t** out) { \
  yrc_parser_symbol_t* sym = state->symbol;\
  yrc_token_t* token = state->token;\
  TYPE* node = malloc(sizeof(*node));\
  printf("prefix " #NAME " " #TYPE " " #KIND "\n");\
  yrc_ast_node_unary_t* asunary = (yrc_ast_node_unary_t*)node;\
  sym;token;\
  if (node == NULL) {\
    return 1;\
  }\
  if (expression(state, BP, &asunary->argument)) {\
    free(node);\
    return 1;\
  }\
  asunary->kind = KIND;\
  do { EXTRA } while(0);\
  *out = (yrc_ast_node_t*)node;\
  return 0;\
}

INFIX(_infix, yrc_ast_node_binary_t, 0, YRC_AST_EXPR_BINARY, {
  node->op = token->info.as_operator;
})
INFIX(_infixr, yrc_ast_node_binary_t, 0, YRC_AST_EXPR_LOGICAL, {
  node->op = token->info.as_operator;
})
INFIX(_assign, yrc_ast_node_binary_t, 0, YRC_AST_EXPR_ASSIGNMENT, {
  node->op = token->info.as_operator;
})
INFIX(_dynget, yrc_ast_node_member_t, 0, YRC_AST_EXPR_MEMBER, {
  /* consume `]` */
  node->computed = 1;
  if (advance(state)) {
    free(asbin->left);
    free(node);
    return 1;
  }
})
PREFIX(_prefix, yrc_ast_node_unary_t, 0, YRC_AST_EXPR_UNARY, {
  node->op = token->info.as_operator;
})


int _prefix_array(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  yrc_ast_node_array_t* node = malloc(sizeof(*node));
  yrc_ast_node_t* item;
  if (yrc_llist_init(&node->elements)) {
    goto cleanup;
  }
  do {
    item = NULL;
    if (expression(state, 0, &item)) {
      goto cleanupfull;
    }
    if (yrc_llist_push(node->elements, item)) {
      goto cleanupfull;
    }
  } while(IS_OP(state->token, COMMA));

  /* consume `]` */
  if (advance(state)) {
    goto cleanupfull;
  }
  *out = (yrc_ast_node_t*)node;
  return 0;
cleanupfull:
  yrc_llist_free(node->elements);
cleanup:
  free(node);
  return 1;
}


int _prefix_object(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  return 0;
}


int _prefix_paren(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  return 0;
}


int _return(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  return 0;
}


int _suffix(yrc_parser_state_t* state, yrc_ast_node_t* left, yrc_ast_node_t** out) {
  return 0;
}


int _ternary(yrc_parser_state_t* state, yrc_ast_node_t* left, yrc_ast_node_t** out) {
  return 0;
}


int _try(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  return 0;
}


int _var(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  return 0;
}


int _while(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  return 0;
}


int _literal(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  yrc_ast_node_literal_t* node = malloc(sizeof(*node));
  if (node == NULL) {
    return 1;
  }
  node->value = state->token;
  *out = (yrc_ast_node_t*)node;
  return 0;
}


int _ident(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  yrc_ast_node_ident_t* node = malloc(sizeof(*node));
  printf("ident\n");
  if (node == NULL) {
    return 1;
  }
  node->name = state->token;
  *out = (yrc_ast_node_t*)node;
  return 0;
}


//STATE("text", TOKEN_TYPE,         SUBTYPE,    LBP, NUD, LED, STD
#define SYMBOLS(XX) \
  XX(stmtwhile,      KEYWORD, as_keyword == YRC_KW_WHILE,       0, NULL,      NULL, _while)\
  XX(stmtdo,         KEYWORD, as_keyword == YRC_KW_DO,          0, NULL,      NULL, _do)\
  XX(stmtif,         KEYWORD, as_keyword == YRC_KW_IF,          0, NULL,      NULL, _if)\
  XX(stmtfor,        KEYWORD, as_keyword == YRC_KW_FOR,         0, NULL,      NULL, _for)\
  XX(stmtbreak,      KEYWORD, as_keyword == YRC_KW_BREAK,       0, NULL,      NULL, _break)\
  XX(stmtcontinue,   KEYWORD, as_keyword == YRC_KW_CONTINUE,    0, NULL,      NULL, _continue)\
  XX(stmtreturn,     KEYWORD, as_keyword == YRC_KW_RETURN,      0, NULL,      NULL, _return)\
  XX(stmttry,        KEYWORD, as_keyword == YRC_KW_TRY,         0, NULL,      NULL, _try)\
  XX(stmtvar,        KEYWORD, as_keyword == YRC_KW_VAR,         0, NULL,      NULL, _var)\
  XX(exprin,         KEYWORD, as_keyword == YRC_KW_IN,         50, NULL,      _infix, NULL)\
  XX(exprvoid,       KEYWORD, as_keyword == YRC_KW_VOID,        0, _prefix,   NULL, NULL)\
  XX(exprtypeof,     KEYWORD, as_keyword == YRC_KW_TYPEOF,      0, _prefix,   NULL, NULL)\
  XX(exprdelete,     KEYWORD, as_keyword == YRC_KW_DELETE,      0, _prefix,   NULL, NULL)\
  XX(exprnew,        KEYWORD, as_keyword == YRC_KW_NEW,         0, _prefix,   NULL, NULL)\
  XX(exprinstanceof, KEYWORD, as_keyword == YRC_KW_INSTANCEOF, 50, NULL,      _infix, NULL)\
  XX(exprlparen,     OPERATOR, as_operator == YRC_OP_LPAREN,     80, _prefix_paren, _call, NULL)\
  XX(lbrace,         OPERATOR, as_operator == YRC_OP_LBRACE,      0, _prefix_object, NULL, _block)\
  XX(exprdot,        OPERATOR, as_operator == YRC_OP_DOT,        80, NULL, _dynget, NULL)\
  XX(exprlbrack,     OPERATOR, as_operator == YRC_OP_LBRACK,     80, _prefix_array, _dynget, NULL)\
  XX(exprmod,        OPERATOR, as_operator == YRC_OP_MOD,        60, NULL, _infix, NULL)\
  XX(exprmul,        OPERATOR, as_operator == YRC_OP_MUL,        60, NULL, _infix, NULL)\
  XX(exprdiv,        OPERATOR, as_operator == YRC_OP_DIV,        60, NULL, _infix, NULL)\
  XX(exprnot,        OPERATOR, as_operator == YRC_OP_NOT,         0, _prefix, NULL, NULL)\
  XX(exprtilde,      OPERATOR, as_operator == YRC_OP_TILDE,       0, _prefix, NULL, NULL)\
  XX(exprincr,       OPERATOR, as_operator == YRC_OP_INCR,      150, _prefix, _suffix, NULL)\
  XX(exprdecr,       OPERATOR, as_operator == YRC_OP_DECR,      150, _prefix, _suffix, NULL)\
  XX(expradd,        OPERATOR, as_operator == YRC_OP_ADD,        50, _prefix, _infix, NULL)\
  XX(exprsub,        OPERATOR, as_operator == YRC_OP_SUB,        50, _prefix, _infix, NULL)\
  XX(exprlesser,     OPERATOR, as_operator == YRC_OP_LESSER,     40, NULL, _infix, NULL)\
  XX(exprgreater,    OPERATOR, as_operator == YRC_OP_GREATER,    40, NULL, _infix, NULL)\
  XX(exprand,        OPERATOR, as_operator == YRC_OP_AND,        40, NULL, _infix, NULL)\
  XX(expror,         OPERATOR, as_operator == YRC_OP_OR,         40, NULL, _infix, NULL)\
  XX(exprxor,        OPERATOR, as_operator == YRC_OP_XOR,        40, NULL, _infix, NULL)\
  XX(exprlshf,       OPERATOR, as_operator == YRC_OP_LSHF,       40, NULL, _infix, NULL)\
  XX(exprrshf,       OPERATOR, as_operator == YRC_OP_RSHF,       40, NULL, _infix, NULL)\
  XX(exprurshf,      OPERATOR, as_operator == YRC_OP_URSHF,      40, NULL, _infix, NULL)\
  XX(exprlessereq,   OPERATOR, as_operator == YRC_OP_LESSEREQ,   40, NULL, _infix, NULL)\
  XX(exprgreatereq,  OPERATOR, as_operator == YRC_OP_GREATEREQ,  40, NULL, _infix, NULL)\
  XX(expreqeq,       OPERATOR, as_operator == YRC_OP_EQEQ,       40, NULL, _infix, NULL)\
  XX(expreqeqeq,     OPERATOR, as_operator == YRC_OP_EQEQEQ,     40, NULL, _infix, NULL)\
  XX(exprnoteq,      OPERATOR, as_operator == YRC_OP_NOTEQ,      40, NULL, _infix, NULL)\
  XX(exprnoteqeq,    OPERATOR, as_operator == YRC_OP_NOTEQEQ,    40, NULL, _infix, NULL)\
  XX(exprlor,        OPERATOR, as_operator == YRC_OP_LOR,        30, NULL, _infixr, NULL)\
  XX(exprland,       OPERATOR, as_operator == YRC_OP_LAND,       30, NULL, _infixr, NULL)\
  XX(exprlxor,       OPERATOR, as_operator == YRC_OP_LXOR,       30, NULL, _infixr, NULL)\
  XX(exprquestion,   OPERATOR, as_operator == YRC_OP_QUESTION,   20, NULL, _ternary, NULL)\
  XX(expreq,         OPERATOR, as_operator == YRC_OP_EQ,         10, NULL, _assign, NULL)\
  XX(expraddeq,      OPERATOR, as_operator == YRC_OP_ADDEQ,      10, NULL, _assign, NULL)\
  XX(exprsubeq,      OPERATOR, as_operator == YRC_OP_SUBEQ,      10, NULL, _assign, NULL)\
  XX(exprandeq,      OPERATOR, as_operator == YRC_OP_ANDEQ,      10, NULL, _assign, NULL)\
  XX(exprxoreq,      OPERATOR, as_operator == YRC_OP_XOREQ,      10, NULL, _assign, NULL)\
  XX(exproreq,       OPERATOR, as_operator == YRC_OP_OREQ,       10, NULL, _assign, NULL)\
  XX(exprmodeq,      OPERATOR, as_operator == YRC_OP_MODEQ,      10, NULL, _assign, NULL)\
  XX(exprmuleq,      OPERATOR, as_operator == YRC_OP_MULEQ,      10, NULL, _assign, NULL)\
  XX(exprdiveq,      OPERATOR, as_operator == YRC_OP_DIVEQ,      10, NULL, _assign, NULL)\
  XX(exprlshfeq,     OPERATOR, as_operator == YRC_OP_LSHFEQ,     10, NULL, _assign, NULL)\
  XX(exprrshfeq,     OPERATOR, as_operator == YRC_OP_RSHFEQ,     10, NULL, _assign, NULL)\
  XX(exprurshfeq,    OPERATOR, as_operator == YRC_OP_URSHFEQ,    10, NULL, _assign, NULL)

static yrc_token_t eof = {YRC_EOF_TOKEN};
static yrc_parser_symbol_t sym_ident = {_ident, NULL, NULL, 0};
static yrc_parser_symbol_t sym_literal = {_literal, NULL, NULL, 0};
static yrc_parser_symbol_t sym_eof = {NULL, NULL, NULL, 0};
#define XX(NAME, TYPE, SUBTYPE, LBP, NUD, LED, STD) \
  static yrc_parser_symbol_t sym_##NAME = {NUD, LED, STD, LBP};
SYMBOLS(XX)
#undef XX

int advance(yrc_parser_state_t* parser) {

  yrc_token_t* token = yrc_llist_shift(parser->tokens);
  if (token == NULL) {
    parser->token = &eof;
    parser->symbol = &sym_eof;
    return 0;
  }

  /* figure out what symbol represents this token */
  parser->token = token;
  parser->saw_newline = token->type == YRC_TOKEN_COMMENT ?
    parser->saw_newline : 0;

  if (token->type == YRC_TOKEN_NUMBER || token->type == YRC_TOKEN_STRING) {
    parser->symbol = &sym_literal;
    return 0;
  } 

  if (token->type == YRC_TOKEN_IDENT) {
    parser->symbol = &sym_ident;
    return 0;
  } 
  
  if (token->type == YRC_TOKEN_COMMENT) {
    return advance(parser);
  }
  
  if (token->type == YRC_TOKEN_WHITESPACE) {
    parser->saw_newline = token->info.as_whitespace.has_newline;
    return advance(parser);
  }
#define STATE(NAME, TYPE, SUBTYPECHECK, LBP, NUD, LED, STD) \
  if (token->type == YRC_TOKEN_##TYPE && token->info.SUBTYPECHECK) {\
    printf(#NAME "\n");\
    parser->symbol = &sym_##NAME;\
    return 0;\
  }
SYMBOLS(STATE)
#undef STATE

  /* unhandled token */
  return 1;
}

int expression(yrc_parser_state_t* parser, int rbp, yrc_ast_node_t** out) {
  yrc_ast_node_t* left = NULL;
  yrc_parser_symbol_t* sym = parser->symbol;
  if (advance(parser)) {
    return 1;
  }
  if (sym->nud(parser, &left)) {
    return 1;
  }
  while (rbp < parser->symbol->lbp) {
    sym = parser->symbol;
    if (advance(parser)) {
      return 1;
    }
    if (sym->led(parser, left, &left)) {
      return 1;
    }
  }

  /* TODO: sequence op */
  if (IS_OP(parser->token, COMMA)) {
  }

  *out = left;
  return 0;
}

int statement(yrc_parser_state_t* parser, yrc_ast_node_t** out) {
  yrc_parser_symbol_t* sym = parser->symbol;
  yrc_ast_node_exprstmt_t* node;
  if (sym->std) {
    if (advance(parser)) {
      return 1;
    }
    return sym->std(parser, out);
  }
  node = malloc(sizeof(*node));
  if (node == NULL) {
    return 1;
  }
  if (expression(parser, 0, &node->expression)) {
    free(node);
    return 1;
  }
  /* TODO: assert consume ; or \n */
  return advance(parser);
}

int statements(yrc_parser_state_t* parser, yrc_llist_t* out) {
  yrc_ast_node_t* stmt;
  while (1) {
    if (IS_OP(parser->token, RBRACE) || IS_EOF(parser->token)) {
      break;
    }
    stmt = NULL;
    if (statement(parser, &stmt)) {
      return 1;
    }
    if (yrc_llist_push(out, stmt)) {
      return 1;
    }
  }
  return 0;
}

int map_ident(void* item, void** out, size_t idx, void* ctx) {
  *out = item;
  return 0;
}

YRC_EXTERN int yrc_parse(yrc_tokenizer_t* tokenizer) {
  yrc_ast_node_program_t* program = malloc(sizeof(*program));
  yrc_parser_state_t parser;
  if (program == NULL) {
    return 1;
  }
  program->kind = YRC_AST_PROGRAM;
  yrc_llist_init(&program->body);
  parser.token = NULL;
  parser.symbol = NULL;
  parser.saw_newline = 0;
  yrc_llist_init(&parser.tokens);
  yrc_llist_map(tokenizer->tokens, parser.tokens, map_ident, NULL);
  if (advance(&parser)) {
    return 1;
  }
  if (statements(&parser, program->body)) {
    return 1;
  }
  yrc_llist_free(parser.tokens);
  return 0;
}
