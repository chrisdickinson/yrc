#include "yrc-common.h"
#include "llist.h"
#include "parser.h"
#include "pool.h"

typedef enum {
  YRC_PARSE_OK,
  E_YRC_PARSE_UNEXPECTED_TOKEN
} yrc_parse_error_type;

typedef struct yrc_parse_error_s {
  yrc_parse_error_type type;
  uint64_t line;
  uint64_t fpos;
  uint64_t col;

  yrc_token_t got;
  yrc_token_t expected;
} yrc_parse_error_t;

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define IS_EOF(K) (K == &eof)
#define IS_OP(K, T) (K->type == YRC_TOKEN_OPERATOR && K->info.as_operator == YRC_OP_##T)
#define IS_KW(K, T) (K->type == YRC_TOKEN_KEYWORD && K->info.as_operator == YRC_KW_##T)
struct yrc_parser_state_s {
  yrc_tokenizer_t*      tokenizer;
  yrc_token_t*          token;
  yrc_parser_symbol_t*  symbol;
  yrc_readcb            readcb;
  yrc_pool_t*           node_pool;
  uint_fast8_t          saw_newline;
  uint_fast8_t          asi;
  yrc_parse_error_t     error;
};

#define CONSUME_CLEAN(state, CHECK, T, CLEANUP)\
  do {\
  if (!CHECK(state->token, T)) { CLEANUP; return 1; }\
  if (advance(state, YRC_ISNT_REGEXP)) { CLEANUP; return 1; }\
  } while(0);
#define CONSUME(state, CHECK, T) CONSUME_CLEAN(state, CHECK, T, {});

static int advance(yrc_parser_state_t*, enum yrc_scan_allow_regexp);
static int expression(yrc_parser_state_t*, int, yrc_ast_node_t**);
static int statement(yrc_parser_state_t*, yrc_ast_node_t**);
static int statements(yrc_parser_state_t*, yrc_llist_t*);
static int _ident(yrc_parser_state_t*, yrc_token_t*, yrc_ast_node_t**);
static yrc_token_t eof = {YRC_TOKEN_EOF, {0, 0, 0}, {0, 0, 0}, {{0, 0, NULL}}};
static yrc_parser_symbol_t sym_eof = {NULL, NULL, NULL, 0};


static int _block(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  if (node == NULL) {
    return 1;
  }
  node->kind = YRC_AST_STMT_BLOCK;
  if (yrc_llist_init(&node->data.as_block.body)) {
    return 1;
  }
  if (statements(state, node->data.as_block.body)) {
    yrc_llist_free(node->data.as_block.body);
    return 1;
  }
  CONSUME_CLEAN(state, IS_OP, RBRACE, {
    yrc_llist_free(node->data.as_block.body);
  });
  *out = (yrc_ast_node_t*)node;
  return 0;
}


static int _break(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  if (node == NULL) {
    return 1;
  }
  node->kind = YRC_AST_STMT_BREAK;
  node->data.as_break.label = (state->token->type == YRC_TOKEN_IDENT) ?
    node->data.as_break.label = state->token :
    NULL;
  *out = (yrc_ast_node_t*)node;
  return 0;
}


static int _do_regexp(yrc_parser_state_t* state, yrc_ast_node_t** out, enum yrc_scan_allow_regexp kind) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  if (node == NULL) {
    return 1;
  }
  *out = (yrc_ast_node_t*)node;
  node->kind = YRC_AST_EXPR_LITERAL;
  node->data.as_literal.value = state->token;
  if (advance(state, YRC_ISNT_REGEXP)) {
    return 1;
  }
  return 0;
}


static int _regexp(yrc_parser_state_t* state, yrc_token_t* orig, yrc_ast_node_t** out) {
  return _do_regexp(state, out, YRC_IS_REGEXP);
}


static int _regexp_eq(yrc_parser_state_t* state, yrc_token_t* orig, yrc_ast_node_t** out) {
  return _do_regexp(state, out, YRC_IS_REGEXP_EQ);
}


static int _call(yrc_parser_state_t* state, yrc_ast_node_t* left, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  yrc_ast_node_t* item;
  if (node == NULL) {
    return 1;
  }
  node->kind = YRC_AST_EXPR_CALL;
  node->data.as_call.callee = left;
  if (yrc_llist_init(&node->data.as_call.arguments)) {
    return 1;
  }
  do {
    item = NULL;
    if (expression(state, 0, &item)) {
      goto cleanup;
    }
    if (yrc_llist_push(node->data.as_call.arguments, item)) {
      goto cleanup;
    }
  } while(IS_OP(state->token, COMMA));

  CONSUME_CLEAN(state, IS_OP, RPAREN, {
    yrc_llist_free(node->data.as_call.arguments);
  });
  *out = (yrc_ast_node_t*)node;
  return 0;

cleanup:
  return 1;
}


static int _continue(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  if (node == NULL) {
    return 1;
  }
  node->kind = YRC_AST_STMT_CONTINUE;
  node->data.as_continue.label = (state->token->type == YRC_TOKEN_IDENT) ?
    node->data.as_continue.label = state->token :
    NULL;
  *out = (yrc_ast_node_t*)node;
  return 0;
}


static int _do(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) return 1;
  node->kind = YRC_AST_STMT_DOWHILE;
  if (statement(state, &node->data.as_do_while.body)) {
    return 1;
  }
  CONSUME(state, IS_KW, WHILE);
  CONSUME(state, IS_OP, LPAREN);
  if (expression(state, 0, &node->data.as_do_while.test)) {
    return 1;
  }
  CONSUME(state, IS_OP, RPAREN);
  return 0;
}


static int _for(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  return 0;
}


static int _if(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) return 1;
  node->kind = YRC_AST_STMT_IF;
  node->data.as_if.alternate = NULL;
  CONSUME(state, IS_OP, LPAREN);
  if (expression(state, 0, &node->data.as_if.test)) {
    return 1;
  }
  CONSUME(state, IS_OP, RPAREN);
  if (statement(state, &node->data.as_if.consequent)) {
    return 1;
  }
  if (!IS_KW(state->token, ELSE)) {
    return 0;
  }
  if (advance(state, YRC_ISNT_REGEXP)) {
    return 1;
  }
  if (statement(state, &node->data.as_if.alternate)) {
    return 1;
  }
  return 0;
}


#define INFIX(NAME, TYPE, RBP_MOD, KIND, EXTRA) \
int NAME(yrc_parser_state_t* state, yrc_ast_node_t* left, yrc_ast_node_t** out) { \
  yrc_parser_symbol_t* sym = state->symbol;\
  yrc_token_t* token = state->token;\
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);\
  sym;token;\
  if (node == NULL) {\
    return 1;\
  }\
  node->kind = KIND;\
  node->data.as_binary.left = left;\
  if (expression(state, state->symbol->lbp + RBP_MOD, &node->data.as_binary.right)) {\
    return 1;\
  }\
  do { EXTRA } while(0);\
  *out = (yrc_ast_node_t*)node;\
  return 0;\
}

#define PREFIX(NAME, TYPE, BP, KIND, EXTRA)\
int NAME(yrc_parser_state_t* state, yrc_token_t* orig, yrc_ast_node_t** out) { \
  yrc_parser_symbol_t* sym = state->symbol;\
  yrc_token_t* token = state->token;\
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);\
  sym;token;\
  if (node == NULL) {\
    return 1;\
  }\
  if (expression(state, BP, &node->data.as_unary.argument)) {\
    return 1;\
  }\
  node->kind = KIND;\
  do { EXTRA } while(0);\
  *out = (yrc_ast_node_t*)node;\
  return 0;\
}

INFIX(_infix, yrc_ast_node_binary_t, 0, YRC_AST_EXPR_BINARY, {
  node->data.as_binary.op = token->info.as_operator;
})
INFIX(_infixr, yrc_ast_node_binary_t, 0, YRC_AST_EXPR_LOGICAL, {
  node->data.as_binary.op = token->info.as_operator;
})
INFIX(_assign, yrc_ast_node_binary_t, 0, YRC_AST_EXPR_ASSIGNMENT, {
  node->data.as_assign.op = token->info.as_operator;
})
INFIX(_dynget, yrc_ast_node_member_t, 0, YRC_AST_EXPR_MEMBER, {
  /* consume `]` */
  node->data.as_member.computed = 1;
  if (advance(state, YRC_ISNT_REGEXP)) {
    return 1;
  }
})
PREFIX(_prefix, yrc_ast_node_unary_t, 0, YRC_AST_EXPR_UNARY, {
  node->data.as_unary.op = token->info.as_operator;
})


static int _prefix_array(yrc_parser_state_t* state, yrc_token_t* orig, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  yrc_ast_node_t* item;
  if (yrc_llist_init(&node->data.as_array.elements)) {
    return 1;
  }
  do {
    item = NULL;
    if (expression(state, 0, &item)) {
      goto cleanup;
    }
    if (yrc_llist_push(node->data.as_array.elements, item)) {
      goto cleanup;
    }
  } while(IS_OP(state->token, COMMA));

  /* consume `]` */
  CONSUME_CLEAN(state, IS_OP, RBRACK, {
    goto cleanup;
  });
  *out = node;
  return 0;
cleanup:
  yrc_llist_free(node->data.as_array.elements);
  return 1;
}


static int _prefix_object(yrc_parser_state_t* state, yrc_token_t* orig, yrc_ast_node_t** out) {
  /* this could be either a block or destructuring */
  /* TODO: support es6 */
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  yrc_ast_node_t* item;
  uint_fast8_t shorthand_prop_ok = 0;
  if (node == NULL) {
    return 1;
  }
  node->kind = YRC_AST_EXPR_OBJECT;
  *out = node;
  if (yrc_llist_init(&node->data.as_array.elements)) {
    return 1;
  }
  do {
    shorthand_prop_ok = 1;
    item = yrc_pool_attain(state->node_pool);
    if (item == NULL) {
      goto cleanup;
    }
    item->kind = YRC_AST_EXPR_PROPERTY;

    if (state->token->type == YRC_TOKEN_IDENT && state->token->info.as_ident.size == 3) {
      if (state->token->info.as_ident.data[1] == 'e' &&
          state->token->info.as_ident.data[1] == 't') {
        switch(state->token->info.as_ident.data[0]) {
          case 's':
          item->data.as_property.type |= YRC_PROP_SPC | YRC_PROP_SET;
          break;
          case 'g':
          item->data.as_property.type |= YRC_PROP_SPC | YRC_PROP_GET;
          break;
          default:
          goto not_getter_or_setter;
        }
        shorthand_prop_ok = 0;
        if (advance(state, YRC_ISNT_REGEXP)) {
          goto cleanup;
        }
      }
    }
not_getter_or_setter:
    if (IS_OP(state->token, LBRACK)) {
      shorthand_prop_ok = 0;
      item->data.as_property.type |= YRC_PROP_SPC | YRC_PROP_COMPUTED;
      if (advance(state, YRC_ISNT_REGEXP)) {
        goto cleanup;
      }
      if (expression(state, 0, &item->data.as_property.key)) {
        goto cleanup;
      }
      CONSUME(state, IS_OP, RBRACK);
    }

    switch (state->token->type) {
      case YRC_TOKEN_KEYWORD:
        shorthand_prop_ok = 0;
        if (yrc_tokenizer_promote_keyword(state->tokenizer, state->token)) {
          goto cleanup;
        }
      case YRC_TOKEN_STRING: 
        shorthand_prop_ok = 0;
      case YRC_TOKEN_IDENT:
        if (_ident(state, state->token, &item->data.as_property.key)) {
          goto cleanup;
        }
      break;
      default:
      /* XXX: unexpected <anything-other-than-words> */
      goto cleanup;
    }

    if (advance(state, YRC_ISNT_REGEXP)) {
      goto cleanup;
    }

    if (IS_OP(state->token, LPAREN)) {
      /* XXX: support ES6 shorthand methods */
      shorthand_prop_ok = 0;
      goto cleanup;
    }

    /* shorthand, like `{x}` or `{z, y: 3}` */
    if (shorthand_prop_ok &&
        (IS_OP(state->token, RBRACE) || IS_OP(state->token, COMMA))) {
      item->data.as_property.expression = item->data.as_property.key;
      item->data.as_property.type |= YRC_PROP_SHORTHAND;
      goto shorthand;
    }

    if (!IS_OP(state->token, COLON)) {
      goto cleanup;
    }

    if (advance(state, YRC_ISNT_REGEXP)) {
      goto cleanup;
    }

    if (expression(state, 0, &item->data.as_property.expression)) {
      goto cleanup;
    }

shorthand:
    if (yrc_llist_push(node->data.as_object.properties, item)) {
      goto cleanup;
    }
  } while(IS_OP(state->token, COMMA));

  /* consume `]` */
  CONSUME_CLEAN(state, IS_OP, RBRACE, {
    goto cleanup;
  });
  return 0;
cleanup:
  yrc_llist_free(node->data.as_array.elements);
  return 1;
}


static int _function(yrc_parser_state_t* state, yrc_token_t* orig, yrc_ast_node_t** out) {
  return 0;
}


static int _functionstmt(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  return 0;
}

static int _prefix_paren(yrc_parser_state_t* state, yrc_token_t* orig, yrc_ast_node_t** out) {
  /* this could be either `(a, b, c)` OR `(a, b) => { }` */
  return 0;
}


static int _return(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  if (node == NULL) {
    return 1;
  }
  node->kind = YRC_AST_STMT_RETURN;
  node->data.as_return.argument = NULL;
  *out = node;
  if (IS_OP(state->token, SEMICOLON)) {
    return advance(state, YRC_ISNT_REGEXP);
  }

  if (state->saw_newline || IS_EOF(state->token) || IS_OP(state->token, RBRACE)) {
    return 0;
  }

  if (expression(state, 0, &node->data.as_return.argument)) {
    return 1;
  }

  if (IS_OP(state->token, SEMICOLON)) {
    return advance(state, YRC_ISNT_REGEXP);
  }

  // TODO: ASI :| chomping `}` should insert semicolon
  if (state->saw_newline || IS_EOF(state->token) || IS_OP(state->token, RBRACE)) {
    return 0;
  }

  return 1;
}


static int _suffix(yrc_parser_state_t* state, yrc_ast_node_t* left, yrc_ast_node_t** out) {
  return 0;
}


static int _ternary(yrc_parser_state_t* state, yrc_ast_node_t* left, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) return 1;
  node->kind = YRC_AST_EXPR_CONDITIONAL;
  node->data.as_conditional.test = left;
  if (expression(state, 0, &node->data.as_conditional.consequent)) {
    return 1;
  }
  CONSUME(state, IS_OP, COLON);
  if (expression(state, 0, &node->data.as_conditional.alternate)) {
    return 1;
  }
  return 0;
}


static int _catch(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) return 1;
  node->kind = YRC_AST_STMT_CATCH;
  CONSUME(state, IS_OP, LPAREN);
  if (expression(state, 0, &node->data.as_catch.param)) {
    return 1;
  }
  CONSUME(state, IS_OP, RPAREN);
  CONSUME(state, IS_OP, LBRACE);
  if (_block(state, &node->data.as_catch.body)) {
    return 1;
  }
  if (IS_KW(state->token, FINALLY)) {
    if (advance(state, YRC_ISNT_REGEXP)) {
      return 1;
    }
    CONSUME(state, IS_OP, LBRACE);
    if (_block(state, &node->data.as_try.finalizer)) {
      return 1;
    }
  }
  return 0;
}


static int _try(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) return 1;
  node->kind = YRC_AST_STMT_TRY;
  CONSUME(state, IS_OP, LBRACE);
  if (_block(state, &node->data.as_try.block)) {
    return 1;
  }

  if (IS_KW(state->token, CATCH)) {
    if (advance(state, YRC_ISNT_REGEXP)) {
      return 1;
    }
    _catch(state, &node->data.as_try.handler);
  } else if (IS_KW(state->token, FINALLY)) {
    node->data.as_try.handler = NULL;
    if (advance(state, YRC_ISNT_REGEXP)) {
      return 1;
    }
    CONSUME(state, IS_OP, LBRACE);
    if (_block(state, &node->data.as_try.finalizer)) {
      return 1;
    }
  } else {
    return 1;
  }

  return 0;
}


static int _var(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  return 0;
}


static int _while(yrc_parser_state_t* state, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) return 1;
  node->kind = YRC_AST_STMT_WHILE;
  CONSUME(state, IS_OP, LPAREN);
  if (expression(state, 0, &node->data.as_while.test)) {
    return 1;
  }
  CONSUME(state, IS_OP, RPAREN);
  if (statement(state, &node->data.as_while.body)) {
    return 1;
  }
  return 0;
}


static int _literal(yrc_parser_state_t* state, yrc_token_t* orig, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  if (node == NULL) {
    return 1;
  }
  node->data.as_literal.value = orig;
  *out = node;
  return 0;
}


static int _ident(yrc_parser_state_t* state, yrc_token_t* orig, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  if (node == NULL) {
    return 1;
  }
  node->data.as_ident.name = orig;
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
  XX(exprfunction,   KEYWORD, as_keyword == YRC_KW_FUNCTION,    0, _function, NULL, _functionstmt)\
  XX(exprvoid,       KEYWORD, as_keyword == YRC_KW_VOID,        0, _prefix,   NULL, NULL)\
  XX(exprtypeof,     KEYWORD, as_keyword == YRC_KW_TYPEOF,      0, _prefix,   NULL, NULL)\
  XX(exprdelete,     KEYWORD, as_keyword == YRC_KW_DELETE,      0, _prefix,   NULL, NULL)\
  XX(exprnew,        KEYWORD, as_keyword == YRC_KW_NEW,         0, _prefix,   NULL, NULL)\
  XX(exprinstanceof, KEYWORD, as_keyword == YRC_KW_INSTANCEOF, 50, NULL,      _infix, NULL)\
  XX(null_else,      KEYWORD, as_keyword == YRC_KW_ELSE,        0, NULL, NULL, NULL)\
  XX(exprlparen,     OPERATOR, as_operator == YRC_OP_LPAREN,     80, _prefix_paren, _call, NULL)\
  XX(lbrace,         OPERATOR, as_operator == YRC_OP_LBRACE,      0, _prefix_object, NULL, _block)\
  XX(exprdot,        OPERATOR, as_operator == YRC_OP_DOT,        80, NULL, _dynget, NULL)\
  XX(exprlbrack,     OPERATOR, as_operator == YRC_OP_LBRACK,     80, _prefix_array, _dynget, NULL)\
  XX(exprmod,        OPERATOR, as_operator == YRC_OP_MOD,        60, NULL, _infix, NULL)\
  XX(exprmul,        OPERATOR, as_operator == YRC_OP_MUL,        60, NULL, _infix, NULL)\
  XX(exprdiv,        OPERATOR, as_operator == YRC_OP_DIV,        60, _regexp, _infix, NULL)\
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
  XX(exprdiveq,      OPERATOR, as_operator == YRC_OP_DIVEQ,      10, _regexp_eq, _assign, NULL)\
  XX(exprlshfeq,     OPERATOR, as_operator == YRC_OP_LSHFEQ,     10, NULL, _assign, NULL)\
  XX(exprrshfeq,     OPERATOR, as_operator == YRC_OP_RSHFEQ,     10, NULL, _assign, NULL)\
  XX(exprurshfeq,    OPERATOR, as_operator == YRC_OP_URSHFEQ,    10, NULL, _assign, NULL)\
  XX(null_rparen,    OPERATOR, as_operator == YRC_OP_RPAREN,      0, NULL, NULL, NULL)\
  XX(null_rbrack,    OPERATOR, as_operator == YRC_OP_RBRACK,      0, NULL, NULL, NULL)\
  XX(null_rbrace,    OPERATOR, as_operator == YRC_OP_RBRACE,      0, NULL, NULL, NULL)\
  XX(null_colon,     OPERATOR, as_operator == YRC_OP_COLON,       0, NULL, NULL, NULL)\
  XX(null_semicolon, OPERATOR, as_operator == YRC_OP_SEMICOLON,   0, NULL, NULL, NULL)

static yrc_parser_symbol_t sym_ident = {_ident, NULL, NULL, 0};
static yrc_parser_symbol_t sym_literal = {_literal, NULL, NULL, 0};
#define XX(NAME, TYPE, SUBTYPE, LBP, NUD, LED, STD) \
  static yrc_parser_symbol_t sym_##NAME = {NUD, LED, STD, LBP};
SYMBOLS(XX)
#undef XX

static int advance(yrc_parser_state_t* parser, enum yrc_scan_allow_regexp allow_regexp) {
  yrc_token_t* token = NULL;
  if (parser->token == &eof) {
    return 0;
  }

  if (yrc_tokenizer_scan(parser->tokenizer, parser->readcb, &token, allow_regexp)) {
    return 1;
  }

  if (token == NULL) {
    parser->token = &eof;
    parser->symbol = &sym_eof;
    return 0;
  }

  /* figure out what symbol represents this token */
  parser->token = token;
  parser->saw_newline = token->type == YRC_TOKEN_COMMENT ?
    parser->saw_newline : MAX(parser->saw_newline - 1, 0);

  if (token->type == YRC_TOKEN_NUMBER ||
      token->type == YRC_TOKEN_STRING ||
      token->type == YRC_TOKEN_REGEXP) {
    parser->symbol = &sym_literal;
    return 0;
  } 

  if (token->type == YRC_TOKEN_IDENT) {
    parser->symbol = &sym_ident;
    return 0;
  } 

  if (token->type == YRC_TOKEN_COMMENT) {
    return advance(parser, allow_regexp);
  }

  if (token->type == YRC_TOKEN_WHITESPACE) {
    parser->saw_newline = token->info.as_whitespace.has_newline << 1;
    return advance(parser, allow_regexp);
  }
#define STATE(NAME, TYPE, SUBTYPECHECK, LBP, NUD, LED, STD) \
  if (token->type == YRC_TOKEN_##TYPE && token->info.SUBTYPECHECK) {\
    parser->symbol = &sym_##NAME;\
    return 0;\
  }
SYMBOLS(STATE)
#undef STATE

  /* unhandled token */
  return 1;
}

static int expression(yrc_parser_state_t* parser, int rbp, yrc_ast_node_t** out) {
  yrc_ast_node_t* left = NULL;
  yrc_parser_symbol_t* sym = parser->symbol;
  yrc_token_t* tok = parser->token;
  enum yrc_scan_allow_regexp type = YRC_ISNT_REGEXP;

  if (IS_OP(tok, DIV)) {
    type = YRC_IS_REGEXP;
  } else if (IS_OP(tok, DIVEQ)) {
    type = YRC_IS_REGEXP_EQ;
  }

  if (advance(parser, type)) {
    return 1;
  }
  if (sym->nud == NULL) {
    return 1;
  }
  if (sym->nud(parser, tok, &left)) {
    return 1;
  }
  while (rbp < parser->symbol->lbp) {
    sym = parser->symbol;
    if (advance(parser, YRC_ISNT_REGEXP)) {
      return 1;
    }
    if (sym->led == NULL) {
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

static int statement(yrc_parser_state_t* parser, yrc_ast_node_t** out) {
  yrc_parser_symbol_t* sym = parser->symbol;
  yrc_ast_node_t* node;
  if (sym->std) {
    if (advance(parser, YRC_ISNT_REGEXP)) {
      return 1;
    }
    return sym->std(parser, out);
  }
  node = yrc_pool_attain(parser->node_pool);
  if (node == NULL) {
    return 1;
  }
  if (expression(parser, 0, &node->data.as_exprstmt.expression)) {
    return 1;
  }
  if (!IS_OP(parser->token, SEMICOLON)) {
    return !(parser->saw_newline || IS_OP(parser->token, RBRACE) || IS_EOF(parser->token));
  }
  /* consume semicolon! */
  if (advance(parser, YRC_ISNT_REGEXP)) {
    return 1;
  }
  return 0;
}

static int statements(yrc_parser_state_t* parser, yrc_llist_t* out) {
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

/* 16K chunks by default */
#define CHUNK_SIZE 256

YRC_EXTERN int yrc_parse(yrc_readcb read) {
  yrc_llist_t* stmts;
  yrc_parser_state_t parser = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    0,
    {
      YRC_PARSE_OK,
      0,
      0,
      0,
      {YRC_TOKEN_EOF, {0, 0, 0}, {0, 0, 0}, {{0, 0, NULL}}},
      {YRC_TOKEN_EOF, {0, 0, 0}, {0, 0, 0}, {{0, 0, NULL}}}
    }
  };
  parser.readcb = read;
  if (yrc_tokenizer_init(&parser.tokenizer, CHUNK_SIZE)) {
    return 1;
  }

  if (yrc_pool_init(&parser.node_pool, sizeof(yrc_ast_node_t))) {
    yrc_tokenizer_free(parser.tokenizer);
    return 1;
  }

  if (advance(&parser, YRC_ISNT_REGEXP)) {
    yrc_tokenizer_free(parser.tokenizer);
    yrc_pool_free(parser.node_pool);
    return 1;
  }

  if (yrc_llist_init(&stmts)) {
    yrc_llist_free(stmts);
    yrc_pool_free(parser.node_pool);
    return 1;
  }

  if (statements(&parser, stmts)) {
    yrc_llist_free(stmts);
    yrc_tokenizer_free(parser.tokenizer);
    yrc_pool_free(parser.node_pool);
    return 1;
  }

  return 0;
}
