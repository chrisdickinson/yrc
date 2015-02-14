#include "yrc-common.h"
#include "llist.h"
#include "parser.h"
#include "pool.h"
#include "traverse.h"

typedef int (*yrc_parser_led_t)(yrc_parser_state_t*, yrc_ast_node_t*, yrc_ast_node_t**);
typedef int (*yrc_parser_nud_t)(yrc_parser_state_t*, yrc_token_t*, yrc_ast_node_t**);
typedef int (*yrc_parser_std_t)(yrc_parser_state_t*, yrc_ast_node_t**, uint_fast8_t);

typedef struct yrc_parser_symbol_s {
  yrc_parser_nud_t        nud;
  yrc_parser_led_t        led;
  yrc_parser_std_t        std;
  uint_fast32_t           lbp;
} yrc_parser_symbol_t;

enum {
  SPECIAL_IN=YRC_NEXT_ADVANCE_FLAG,
  CONSUME_SEMICOLON=YRC_NEXT_ADVANCE_FLAG<<1
};

/* extends yrc_error_t */
typedef struct yrc_parse_error_s {
  YRC_ERROR_BASE;

  yrc_token_t got;
  yrc_token_t expected;
} yrc_parse_error_t;

int nop(char* relationship, yrc_ast_node_type type, void* ctx) {
  return 0;
}


#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define IS_EOF(K) (K == &eof)
#define IS_OP(K, T) (K->type == YRC_TOKEN_OPERATOR && K->info.as_operator == YRC_OP_##T)
#define IS_KW(K, T) (K->type == YRC_TOKEN_KEYWORD && K->info.as_keyword == YRC_KW_##T)
struct yrc_parser_state_s {
  yrc_tokenizer_t*      tokenizer;
  yrc_token_t*          last;
  yrc_token_t*          token;
  yrc_parser_symbol_t*  symbol;
  yrc_readcb            readcb;
  yrc_pool_t*           node_pool;
  uint_fast8_t          saw_newline;
  uint_fast8_t          allow_comma;
  yrc_error_t**         errorptr;
};

typedef struct yrc_parse_response_priv_s {
  yrc_parse_response_t  response;
  yrc_tokenizer_t*      tokenizer;
  yrc_pool_t*           node_pool;
} yrc_parse_response_priv_t;

#define CONSUME_CLEAN(state, CHECK, T, CLEANUP)\
  do {\
  if (!CHECK(state->token, T)) { CLEANUP; return 1; }\
  if (advance(state, YRC_ISNT_REGEXP)) { CLEANUP; return 1; }\
  } while(0);
#define CONSUME(state, CHECK, T) CONSUME_CLEAN(state, CHECK, T, {});

static int advance(yrc_parser_state_t*, uint_fast8_t);
static int commaexpression(yrc_parser_state_t*, uint_fast32_t, yrc_ast_node_t**, uint_fast8_t);
static int expression(yrc_parser_state_t*, uint_fast32_t, yrc_ast_node_t**, uint_fast8_t);
static int statement(yrc_parser_state_t*, yrc_ast_node_t**, uint_fast8_t);
static int statements(yrc_parser_state_t*, yrc_llist_t*);
static int _ident(yrc_parser_state_t*, yrc_token_t*, yrc_ast_node_t**);
static yrc_token_t eof = {YRC_TOKEN_EOF, {0, 0, 0}, {0, 0, 0}, {{0, 0, NULL}}};
static yrc_parser_symbol_t sym_eof = {NULL, NULL, NULL, 0};


static int _block(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
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


static int _break(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  if (node == NULL) {
    return 1;
  }
  node->kind = YRC_AST_STMT_BREAK;

  /* XXX: need to advance if there is a label */
  /* XXX: ASI */
  node->data.as_break.label = (state->token->type == YRC_TOKEN_IDENT) ?
    node->data.as_break.label = state->token :
    NULL;
  *out = (yrc_ast_node_t*)node;
  
  return 0;
}


static int _throw(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  if (node == NULL) {
    return 1;
  }
  node->kind = YRC_AST_STMT_THROW;

  /* XXX: ASI */
  if (commaexpression(state, 0, &node->data.as_throw.argument, 0)) {
    return 1;
  }
  *out = node;
  
  return 0;
}


static int _do_regexp(yrc_parser_state_t* state, yrc_ast_node_t** out, yrc_scan_allow_regexp kind) {
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
  if (!IS_OP(state->token, RPAREN))
  do {
    item = NULL;
    if (expression(state, 0, &item, 0)) {
      goto cleanup;
    }
    if (yrc_llist_push(node->data.as_call.arguments, item)) {
      goto cleanup;
    }
    if (!IS_OP(state->token, COMMA)) {
      break;
    }
    if (advance(state, YRC_ISNT_REGEXP)) {
      return 1;
    }
  } while(1);

  CONSUME_CLEAN(state, IS_OP, RPAREN, {
    yrc_llist_free(node->data.as_call.arguments);
  });
  *out = (yrc_ast_node_t*)node;
  return 0;

cleanup:
  return 1;
}


static int _continue(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  if (node == NULL) {
    return 1;
  }

  /* XXX: ASI */
  node->kind = YRC_AST_STMT_CONTINUE;
  node->data.as_continue.label = (state->token->type == YRC_TOKEN_IDENT) ?
    node->data.as_continue.label = state->token :
    NULL;
  *out = (yrc_ast_node_t*)node;
  return 0;
}


static int _do(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) return 1;
  node->kind = YRC_AST_STMT_DOWHILE;
  if (statement(state, &node->data.as_do_while.body, CONSUME_SEMICOLON)) {
    return 1;
  }
  CONSUME(state, IS_KW, WHILE);
  CONSUME(state, IS_OP, LPAREN);
  if (commaexpression(state, 0, &node->data.as_do_while.test, 0)) {
    return 1;
  }
  CONSUME(state, IS_OP, RPAREN);
  return 0;
}


static int _parse_for(yrc_parser_state_t* state, yrc_ast_node_t* init, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) return 1;
  node->kind = YRC_AST_STMT_FOR;
  node->data.as_for.init = init;
  CONSUME(state, IS_OP, SEMICOLON);
  if (IS_OP(state->token, SEMICOLON)) {
     node->data.as_for.test = NULL;
  } else if (commaexpression(state, 0, &node->data.as_for.test, 0)) {
    return 1;
  }
  CONSUME(state, IS_OP, SEMICOLON);
  if (IS_OP(state->token, RPAREN)) {
    node->data.as_for.update = NULL;
  } else if (commaexpression(state, 0, &node->data.as_for.update, 0)) {
    return 1;
  }
  CONSUME(state, IS_OP, RPAREN);
  if (statement(state, &node->data.as_for.body, CONSUME_SEMICOLON)) {
    return 1;
  }
  return 0;
}


static int _parse_forinof(yrc_parser_state_t* state, yrc_ast_node_t* init, yrc_ast_node_t** out, yrc_ast_node_type type) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) return 1;
  node->kind = type;
  node->data.as_for_in.left = init;
  CONSUME(state, IS_KW, IN);
  if (commaexpression(state, 0, &node->data.as_for_in.right, 0)) {
    return 1;
  }
  CONSUME(state, IS_OP, RPAREN);
  if (statement(state, &node->data.as_for_in.body, CONSUME_SEMICOLON)) {
    return 1;
  }
  return 0;
}


static int _for(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
  /*for (var x in*/
  /*for (var x = 0 in*/
  /*for (var x of*/
  /*for (var x;;)*/
  /*for (;;)*/
  yrc_ast_node_t* node = NULL;
  CONSUME(state, IS_OP, LPAREN);

  if (IS_KW(state->token, VAR) ||
      IS_KW(state->token, LET) ||
      IS_KW(state->token, CONST)) {
    if (statement(state, &node, SPECIAL_IN)) {
      return 1;
    }
    if (IS_KW(state->token, IN)) {
      return _parse_forinof(state, node, out, YRC_AST_STMT_FORIN);
    }
  } else if (IS_OP(state->token, SEMICOLON)) {
    ;
  } else if (commaexpression(state, 0, &node, SPECIAL_IN)) {
    return 1;
  }

  if (IS_OP(state->token, SEMICOLON)) {
    return _parse_for(state, node, out);
  } 

  if (IS_KW(state->token, IN)) {
    return _parse_forinof(state, node, out, YRC_AST_STMT_FORIN);
  }

  return 1;
}


static int _if(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) return 1;
  node->kind = YRC_AST_STMT_IF;
  node->data.as_if.alternate = NULL;
  CONSUME(state, IS_OP, LPAREN);
  if (commaexpression(state, 0, &node->data.as_if.test, 0)) {
    return 1;
  }
  CONSUME(state, IS_OP, RPAREN);
  if (statement(state, &node->data.as_if.consequent, CONSUME_SEMICOLON)) {
    return 1;
  }
  if (!IS_KW(state->token, ELSE)) {
    return 0;
  }
  if (advance(state, YRC_ISNT_REGEXP)) {
    return 1;
  }
  if (statement(state, &node->data.as_if.alternate, CONSUME_SEMICOLON)) {
    return 1;
  }
  return 0;
}


#define INFIX(NAME, TYPE, RBP_MOD, KIND, EXTRA) \
static int NAME(yrc_parser_state_t* state, yrc_ast_node_t* left, yrc_ast_node_t** out) { \
  yrc_token_t* token = state->last;\
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);\
  if (node == NULL) {\
    return 1;\
  }\
  node->kind = KIND;\
  node->data.as_binary.left = left;\
  if (expression(state, state->symbol->lbp + RBP_MOD, &node->data.as_binary.right, 0)) {\
    return 1;\
  }\
  do { EXTRA } while(0);\
  *out = (yrc_ast_node_t*)node;\
  return 0;\
}

#define PREFIX(NAME, TYPE, BP, KIND, EXTRA)\
static int NAME(yrc_parser_state_t* state, yrc_token_t* orig, yrc_ast_node_t** out) { \
  yrc_token_t* token = state->token;\
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);\
  if (node == NULL) {\
    return 1;\
  }\
  if (expression(state, BP, &node->data.as_unary.argument, 0)) {\
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
INFIX(_infixr, yrc_ast_node_binary_t, -1, YRC_AST_EXPR_LOGICAL, {
  node->data.as_binary.op = token->info.as_operator;
})
INFIX(_assign, yrc_ast_node_binary_t, -1, YRC_AST_EXPR_ASSIGNMENT, {
  node->data.as_assign.op = token->info.as_operator;
})
PREFIX(_prefix, yrc_ast_node_unary_t, 0, YRC_AST_EXPR_UNARY, {
  node->data.as_unary.op = token->info.as_operator;
})


static int _dynget(yrc_parser_state_t* state, yrc_ast_node_t* left, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  if (node == NULL) return 1;
  node->kind = YRC_AST_EXPR_MEMBER;
  node->data.as_member.computed = 1;
  node->data.as_member.object = left;
  if (commaexpression(state, 0, &node->data.as_member.property, 0)) {
    return 1;
  }
  CONSUME(state, IS_OP, RBRACK);
  *out = node;
  return 0;
}


static int _get(yrc_parser_state_t* state, yrc_ast_node_t* left, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  if (node == NULL) return 1;
  node->kind = YRC_AST_EXPR_MEMBER;
  node->data.as_member.computed = 0;
  if (state->token->type != YRC_TOKEN_IDENT) {
    return 1;
  }
  node->data.as_member.object = left;
  if (_ident(state, state->token, &node->data.as_member.property)) {
    return 1;
  }
  if (advance(state, YRC_ISNT_REGEXP)) {
    return 1;
  }
  *out = node;
  return 0;
}


static int _prefix_array(yrc_parser_state_t* state, yrc_token_t* orig, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  yrc_ast_node_t* item;
  if (yrc_llist_init(&node->data.as_array.elements)) {
    return 1;
  }
  node->kind = YRC_AST_EXPR_ARRAY;
  do {
    if (IS_OP(state->token, RBRACK)) {
      break;
    }
    if (IS_OP(state->token, COMMA)) {
      if (advance(state, YRC_ISNT_REGEXP)) {
        return 1;
      }
      continue;
    }
    item = NULL;
    if (expression(state, 0, &item, 0)) {
      goto cleanup;
    }
    if (yrc_llist_push(node->data.as_array.elements, item)) {
      goto cleanup;
    }
    CONSUME_CLEAN(state, IS_OP, COMMA, { break; });
  } while(1);

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

  if (IS_OP(state->token, RBRACE)) {
    node->data.as_object.properties = NULL;
    return advance(state, YRC_ISNT_REGEXP);
  }
  if (yrc_llist_init(&node->data.as_object.properties)) {
    return 1;
  }
  do {
    shorthand_prop_ok = 1;
    item = yrc_pool_attain(state->node_pool);
    if (item == NULL) {
      goto cleanup;
    }
    item->kind = YRC_AST_EXPR_PROPERTY;

    /* XXX: todo, support "{get: x}" */
    if (0)
    if (state->token->type == YRC_TOKEN_IDENT && state->token->info.as_ident.size == 3) {
      if (state->token->info.as_ident.data[1] == 'e' &&
          state->token->info.as_ident.data[2] == 't') {
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
      if (commaexpression(state, 0, &item->data.as_property.key, 0)) {
        goto cleanup;
      }
      CONSUME(state, IS_OP, RBRACK);
    } else {
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

    if (expression(state, 0, &item->data.as_property.expression, 0)) {
      goto cleanup;
    }

shorthand:
    if (yrc_llist_push(node->data.as_object.properties, item)) {
      goto cleanup;
    }

    if (!IS_OP(state->token, COMMA)) {
      break;
    }

    if (advance(state, YRC_ISNT_REGEXP)) {
      return 1;
    }
  } while(1);

  /* consume `}` */
  CONSUME_CLEAN(state, IS_OP, RBRACE, {
    goto cleanup;
  });
  return 0;
cleanup:
  yrc_llist_free(node->data.as_object.properties);
  return 1;
}


static int _parameters(yrc_parser_state_t* state, yrc_ast_node_function_t* fn) {
  if (!IS_OP(state->token, RPAREN))
  do {
    yrc_ast_node_t* expr;
    yrc_ast_node_t* def;
    if (expression(state, 10, &expr, 0)) {
      return 1;
    }
    if (IS_OP(state->token, EQ)) {
      if (expression(state, 0, &def, 0)) {
        return 1;
      }
    } else {
      def = NULL;
    }
    if (yrc_llist_push(fn->params, expr)) {
      return 1;
    }
    if (yrc_llist_push(fn->defaults, def)) {
      return 1;
    }
    if (!IS_OP(state->token, COMMA)) {
      break;
    }
    if (advance(state, YRC_ISNT_REGEXP)) {
      return 1;
    }
  } while (1);

  CONSUME(state, IS_OP, RPAREN);
  return 0;
}


static int _parse_function(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t needs_ident, yrc_ast_node_type kind) {
  yrc_ast_node_t* node;
  int err = 1;
  /* TODO: generator functions */
  if (needs_ident && state->token->type != YRC_TOKEN_IDENT) {
    return 1;
  }
  node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) {
    return 1;
  }
  if (yrc_llist_init(&node->data.as_function.params)) {
    return 1;
  }
  if (yrc_llist_init(&node->data.as_function.defaults)) {
    yrc_llist_free(node->data.as_function.params);
    return 1;
  }
  node->kind = kind;
  if (needs_ident && state->token->type != YRC_TOKEN_IDENT) {
    return 1;
  }

  if (state->token->type == YRC_TOKEN_IDENT) {
    node->data.as_function.id = state->token;
    if (advance(state, YRC_ISNT_REGEXP)) {
      goto cleanup;
    }
  } else {
    node->data.as_function.id = NULL;
  }

  CONSUME_CLEAN(state, IS_OP, LPAREN, { goto cleanup; });
  if (_parameters(state, &node->data.as_function)) {
    goto cleanup;
  }
  CONSUME_CLEAN(state, IS_OP, LBRACE, { goto cleanup; });
  if (_block(state, &node->data.as_function.body, 0)) {
    goto cleanup;
  }
  return 0;
cleanup:
  yrc_llist_free(node->data.as_function.params);
  yrc_llist_free(node->data.as_function.defaults);
  return err;
}


static int _function(yrc_parser_state_t* state, yrc_token_t* orig, yrc_ast_node_t** out) {
  if (_parse_function(state, out, 0, YRC_AST_EXPR_FUNCTION)) {
    return 1;
  }
  return 0;
}


static int _functionstmt(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
  return _parse_function(state, out, 1, YRC_AST_DECL_FUNCTION);
}


static int _prefix_paren(yrc_parser_state_t* state, yrc_token_t* orig, yrc_ast_node_t** out) {
  /* this could be either `(a, b, c)` OR `(a, b) => { }` */
  if (commaexpression(state, 0, out, 0)) {
    return 1;
  }
  (*out)->has_parens = 1;
  CONSUME(state, IS_OP, RPAREN);
  return 0;
}


static int _return(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
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

  if (commaexpression(state, 0, &node->data.as_return.argument, 0)) {
    return 1;
  }

  /* XXX: ASI? */
  if (IS_OP(state->token, SEMICOLON)) {
    return advance(state, YRC_ISNT_REGEXP);
  }

  if (state->saw_newline || IS_EOF(state->token) || IS_OP(state->token, RBRACE)) {
    return 0;
  }

  return 1;
}


static int _suffix(yrc_parser_state_t* state, yrc_ast_node_t* left, yrc_ast_node_t** out, yrc_operator_t op) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  if (node == NULL) {
    return 1;
  }
  node->kind = YRC_AST_EXPR_UPDATE;
  node->data.as_update.op = op;
  node->data.as_update.argument = left;
  node->data.as_update.prefix = 0;
  *out = node;
  return 0;
}


static int _suffix_min(yrc_parser_state_t* state, yrc_ast_node_t* left, yrc_ast_node_t** out) {
  return _suffix(state, left, out, YRC_OP_DECR);
}

static int _suffix_add(yrc_parser_state_t* state, yrc_ast_node_t* left, yrc_ast_node_t** out) {
  return _suffix(state, left, out, YRC_OP_INCR);
}

static int _ternary(yrc_parser_state_t* state, yrc_ast_node_t* left, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) return 1;
  node->kind = YRC_AST_EXPR_CONDITIONAL;
  node->data.as_conditional.test = left;
  if (commaexpression(state, 0, &node->data.as_conditional.consequent, 0)) {
    return 1;
  }
  CONSUME(state, IS_OP, COLON);
  if (expression(state, 0, &node->data.as_conditional.alternate, 0)) {
    return 1;
  }
  return 0;
}


static int _catch(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) return 1;
  node->data.as_try.finalizer = NULL;
  node->data.as_try.handler = NULL;

  node->kind = YRC_AST_CLSE_CATCH;
  CONSUME(state, IS_OP, LPAREN);
  if (expression(state, 0, &node->data.as_catch.param, 0)) {
    return 1;
  }
  CONSUME(state, IS_OP, RPAREN);
  CONSUME(state, IS_OP, LBRACE);
  if (_block(state, &node->data.as_catch.body, 0)) {
    return 1;
  }
  if (IS_KW(state->token, FINALLY)) {
    if (advance(state, YRC_ISNT_REGEXP)) {
      return 1;
    }
    CONSUME(state, IS_OP, LBRACE);
    if (_block(state, &node->data.as_try.finalizer, 0)) {
      return 1;
    }
  }
  return 0;
}


static int _trystmt(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) return 1;
  node->kind = YRC_AST_STMT_TRY;
  CONSUME(state, IS_OP, LBRACE);
  if (_block(state, &node->data.as_try.block, 0)) {
    return 1;
  }

  if (IS_KW(state->token, CATCH)) {
    if (advance(state, YRC_ISNT_REGEXP)) {
      return 1;
    }
    return _catch(state, &node->data.as_try.handler, 0);
  } else if (IS_KW(state->token, FINALLY)) {
    node->data.as_try.handler = NULL;
    if (advance(state, YRC_ISNT_REGEXP)) {
      return 1;
    }
    CONSUME(state, IS_OP, LBRACE);
    if (_block(state, &node->data.as_try.finalizer, 0)) {
      return 1;
    }
  } else {
    return 1;
  }

  return 0;
}

static int _cases(yrc_parser_state_t* state, yrc_llist_t* cases) {
  yrc_ast_node_t* node;
  do {
    if (IS_OP(state->token, RBRACE)) {
      break;
    }
    node = yrc_pool_attain(state->node_pool);
    if (node == NULL) {
      return 1;
    }
    node->kind = YRC_AST_CLSE_CASE;
    if (IS_KW(state->token, CASE)) {
      CONSUME(state, IS_KW, CASE);
      if (commaexpression(state, 0, &node->data.as_case.test, 0)) {
        return 1;
      }
    } else if (IS_KW(state->token, DEFAULT)) {
      CONSUME(state, IS_KW, DEFAULT);
      node->data.as_case.test = NULL;
    } else {
      return 1;
    }
    CONSUME(state, IS_OP, COLON);
    if (yrc_llist_init(&node->data.as_case.consequent)) {
      return 1;
    }

    if (statements(state, node->data.as_case.consequent)) {
      return 1;
    }

    if (yrc_llist_push(cases, node)) {
      return 1;
    }
    node = NULL;
  } while(1);

  return 0;
}

static int _switchstmt(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) return 1;
  node->kind = YRC_AST_STMT_SWITCH;
  if (yrc_llist_init(&node->data.as_switch.cases)) {
    return 1;
  }

  CONSUME_CLEAN(state, IS_OP, LPAREN, { goto cleanup; });
  if (commaexpression(state, 0, &node->data.as_switch.discriminant, YRC_ISNT_REGEXP)) {
    goto cleanup;
  }
  CONSUME_CLEAN(state, IS_OP, RPAREN, { goto cleanup; });
  CONSUME_CLEAN(state, IS_OP, LBRACE, { goto cleanup; });
  if (_cases(state, node->data.as_switch.cases)) {
    return 1;
  }
  CONSUME_CLEAN(state, IS_OP, RBRACE, { goto cleanup; });
  return 0;
cleanup:
  yrc_llist_free(node->data.as_switch.cases);
  return 1;
}

static int _decl(yrc_parser_state_t* state, yrc_ast_node_t** out, yrc_var_type type, uint_fast8_t flags) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  yrc_ast_node_t* item = NULL;
  *out = node;
  if (node == NULL) return 1;
  node->kind = YRC_AST_DECL_VAR;
  node->data.as_var.type = type;
  if (yrc_llist_init(&node->data.as_var.declarations)) {
    return 1;
  }

  do {
    item = yrc_pool_attain(state->node_pool);
    if (item == NULL) goto cleanup;
    item->kind = YRC_AST_CLSE_VAR;
    item->data.as_vardecl.init = NULL;

    /* don't take any assignment operations */
    if (expression(state, 10, &item->data.as_vardecl.id, flags)) {
      return 1;
    }
    if (IS_OP(state->token, EQ)) {
      if (advance(state, YRC_ISNT_REGEXP)) {
        return 1;
      }
      if (expression(state, 0, &item->data.as_vardecl.init, flags)) {
        return 1;
      }
    }

    if (yrc_llist_push(node->data.as_var.declarations, item)) {
      return 1;
    }

    if (!IS_OP(state->token, COMMA)) {
      break;
    }

    if (advance(state, YRC_ISNT_REGEXP)) {
      return 1;
    }
  } while(1);

  /* XXX: ASI */

  return 0;
cleanup:
  return 1;
}


static int _var(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
  return _decl(state, out, YRC_VARTYPE_VAR, flags);
}


static int _const(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
  return _decl(state, out, YRC_VARTYPE_CONST, flags);
}


static int _let(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
  return _decl(state, out, YRC_VARTYPE_LET, flags);
}


static int _while(yrc_parser_state_t* state, yrc_ast_node_t** out, uint_fast8_t flags) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  *out = node;
  if (node == NULL) return 1;
  node->kind = YRC_AST_STMT_WHILE;
  CONSUME(state, IS_OP, LPAREN);
  if (commaexpression(state, 0, &node->data.as_while.test, 0)) {
    return 1;
  }
  CONSUME(state, IS_OP, RPAREN);
  if (statement(state, &node->data.as_while.body, CONSUME_SEMICOLON)) {
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
  node->kind = YRC_AST_EXPR_LITERAL;
  *out = node;
  return 0;
}


static int _ident(yrc_parser_state_t* state, yrc_token_t* orig, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  if (node == NULL) {
    return 1;
  }
  node->data.as_ident.name = orig;
  node->kind = YRC_AST_EXPR_IDENTIFIER;
  *out = (yrc_ast_node_t*)node;
  return 0;
}


static int _this(yrc_parser_state_t* state, yrc_token_t* orig, yrc_ast_node_t** out) {
  yrc_ast_node_t* node = yrc_pool_attain(state->node_pool);
  if (node == NULL) {
    return 1;
  }
  node->kind = YRC_AST_EXPR_THIS;
  *out = (yrc_ast_node_t*)node;
  return 0;
}

#define SYMBOLS(XX) \
  XX(stmtthis,       KEYWORD, as_keyword == YRC_KW_THIS,        0, _this,     NULL, NULL)\
  XX(stmtwhile,      KEYWORD, as_keyword == YRC_KW_WHILE,       0, NULL,      NULL, _while)\
  XX(stmtdo,         KEYWORD, as_keyword == YRC_KW_DO,          0, NULL,      NULL, _do)\
  XX(stmtif,         KEYWORD, as_keyword == YRC_KW_IF,          0, NULL,      NULL, _if)\
  XX(stmtfor,        KEYWORD, as_keyword == YRC_KW_FOR,         0, NULL,      NULL, _for)\
  XX(stmtthrow,      KEYWORD, as_keyword == YRC_KW_THROW,       0, NULL,      NULL, _throw)\
  XX(stmtbreak,      KEYWORD, as_keyword == YRC_KW_BREAK,       0, NULL,      NULL, _break)\
  XX(stmtcontinue,   KEYWORD, as_keyword == YRC_KW_CONTINUE,    0, NULL,      NULL, _continue)\
  XX(stmtreturn,     KEYWORD, as_keyword == YRC_KW_RETURN,      0, NULL,      NULL, _return)\
  XX(stmttry,        KEYWORD, as_keyword == YRC_KW_TRY,         0, NULL,      NULL, _trystmt)\
  XX(stmtvar,        KEYWORD, as_keyword == YRC_KW_VAR,         0, NULL,      NULL, _var)\
  XX(stmtlet,        KEYWORD, as_keyword == YRC_KW_LET,         0, NULL,      NULL, _let)\
  XX(stmtconst,      KEYWORD, as_keyword == YRC_KW_CONST,       0, NULL,      NULL, _const)\
  XX(stmtswitch,     KEYWORD, as_keyword == YRC_KW_SWITCH,      0, NULL,      NULL, _switchstmt)\
  XX(exprin,         KEYWORD, as_keyword == YRC_KW_IN,         50, NULL,      _infix, NULL)\
  XX(exprfunction,   KEYWORD, as_keyword == YRC_KW_FUNCTION,    0, _function, NULL, _functionstmt)\
  XX(exprvoid,       KEYWORD, as_keyword == YRC_KW_VOID,        0, _prefix,   NULL, NULL)\
  XX(exprtypeof,     KEYWORD, as_keyword == YRC_KW_TYPEOF,      0, _prefix,   NULL, NULL)\
  XX(exprdelete,     KEYWORD, as_keyword == YRC_KW_DELETE,      0, _prefix,   NULL, NULL)\
  XX(exprnew,        KEYWORD, as_keyword == YRC_KW_NEW,         0, _prefix,   NULL, NULL)\
  XX(exprinstanceof, KEYWORD, as_keyword == YRC_KW_INSTANCEOF, 50, NULL,      _infix, NULL)\
  XX(null_else,      KEYWORD, as_keyword == YRC_KW_ELSE,        0, NULL, NULL, NULL)\
  XX(null_catch,     KEYWORD, as_keyword == YRC_KW_CATCH,       0, NULL, NULL, NULL)\
  XX(null_finally,   KEYWORD, as_keyword == YRC_KW_FINALLY,     0, NULL, NULL, NULL)\
  XX(null_case,      KEYWORD, as_keyword == YRC_KW_CASE,        0, NULL, NULL, NULL)\
  XX(null_default,   KEYWORD, as_keyword == YRC_KW_DEFAULT,     0, NULL, NULL, NULL)\
  XX(exprlparen,     OPERATOR, as_operator == YRC_OP_LPAREN,     80, _prefix_paren, _call, NULL)\
  XX(lbrace,         OPERATOR, as_operator == YRC_OP_LBRACE,      0, _prefix_object, NULL, _block)\
  XX(exprdot,        OPERATOR, as_operator == YRC_OP_DOT,        80, NULL, _get, NULL)\
  XX(exprlbrack,     OPERATOR, as_operator == YRC_OP_LBRACK,     80, _prefix_array, _dynget, NULL)\
  XX(exprmod,        OPERATOR, as_operator == YRC_OP_MOD,        60, NULL, _infix, NULL)\
  XX(exprmul,        OPERATOR, as_operator == YRC_OP_MUL,        60, NULL, _infix, NULL)\
  XX(exprdiv,        OPERATOR, as_operator == YRC_OP_DIV,        60, _regexp, _infix, NULL)\
  XX(exprnot,        OPERATOR, as_operator == YRC_OP_NOT,         0, _prefix, NULL, NULL)\
  XX(exprtilde,      OPERATOR, as_operator == YRC_OP_TILDE,       0, _prefix, NULL, NULL)\
  XX(exprincr,       OPERATOR, as_operator == YRC_OP_INCR,      150, _prefix, _suffix_add, NULL)\
  XX(exprdecr,       OPERATOR, as_operator == YRC_OP_DECR,      150, _prefix, _suffix_min, NULL)\
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
  XX(null_comma,     OPERATOR, as_operator == YRC_OP_COMMA,       0, NULL, NULL, NULL)\
  XX(null_semicolon, OPERATOR, as_operator == YRC_OP_SEMICOLON,   0, NULL, NULL, NULL)

static yrc_parser_symbol_t sym_special_in = {NULL, NULL, NULL, 0};
static yrc_parser_symbol_t sym_ident = {_ident, NULL, NULL, 0};
static yrc_parser_symbol_t sym_literal = {_literal, NULL, NULL, 0};
#define XX(NAME, TYPE, SUBTYPE, LBP, NUD, LED, STD) \
  static yrc_parser_symbol_t sym_##NAME = {NUD, LED, STD, LBP};
SYMBOLS(XX)
#undef XX

static int advance(yrc_parser_state_t* parser, uint_fast8_t flags) {
  yrc_token_t* token = NULL;
  uint_fast8_t allow_regexp = flags & (YRC_IS_REGEXP | YRC_IS_REGEXP_EQ);
  if (parser->token == &eof) {
    return 0;
  }

  if (yrc_tokenizer_scan(parser->tokenizer, parser->readcb, &token, allow_regexp)) {
    return 1;
  }

  if (token == NULL) {
    parser->last = parser->token;
    parser->token = &eof;
    parser->symbol = &sym_eof;
    return 0;
  }

  /* figure out what symbol represents this token */
  parser->saw_newline = token->type == YRC_TOKEN_COMMENT ?
    parser->saw_newline : MAX(parser->saw_newline - 1, 0);

  if (token->type == YRC_TOKEN_WHITESPACE) {
    parser->saw_newline = token->info.as_whitespace.has_newline << 1;
    return advance(parser, flags);
  }

  if (token->type == YRC_TOKEN_COMMENT) {
    return advance(parser, flags);
  }

  parser->last = parser->token;
  parser->token = token;
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

  if ((flags & SPECIAL_IN) &&
      token->type == YRC_TOKEN_KEYWORD &&
      token->info.as_keyword == YRC_KW_IN) {
    parser->symbol = &sym_special_in;
    return 0;
  }

#define STATE(NAME, TYPE, SUBTYPECHECK, LBP, NUD, LED, STD) \
  if (token->type == YRC_TOKEN_##TYPE && token->info.SUBTYPECHECK) {\
    parser->symbol = &sym_##NAME;\
    return 0;\
  }
SYMBOLS(STATE)
#undef STATE
  /* unhandled token */
  UNREACHABLE();
  return 1;
}

static int expression(yrc_parser_state_t* parser, uint_fast32_t rbp, yrc_ast_node_t** out, uint_fast8_t flags) {
  yrc_ast_node_t* left = NULL;
  yrc_parser_symbol_t* sym = parser->symbol;
  yrc_token_t* tok = parser->token;
  yrc_scan_allow_regexp type = YRC_ISNT_REGEXP;

  if (IS_OP(tok, DIV)) {
    type = YRC_IS_REGEXP;
  } else if (IS_OP(tok, DIVEQ)) {
    type = YRC_IS_REGEXP_EQ;
  }

  if (advance(parser, type | flags)) {
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
    if (advance(parser, flags)) {
      return 1;
    }
    if (sym->led == NULL) {
      return 1;
    }
    if (sym->led(parser, left, &left)) {
      return 1;
    }
  }

  *out = left;
  return 0;
}


int commaexpression(yrc_parser_state_t* parser, uint_fast32_t rbp, yrc_ast_node_t** out, uint_fast8_t flags) {
  if (expression(parser, rbp, out, flags)) {
    return 1;
  }
  if (IS_OP(parser->token, COMMA)) {
    yrc_ast_node_t* seq = NULL;
    seq = yrc_pool_attain(parser->node_pool);
    if (seq == NULL) {
      return 1;
    }
    seq->kind = YRC_AST_EXPR_SEQUENCE;
    seq->data.as_sequence.left = *out;
    *out = seq;
    if (advance(parser, YRC_ISNT_REGEXP)) {
      return 1;
    }
    return commaexpression(parser, 0, &seq->data.as_sequence.right, 0);
  }
  return 0;
}


static int statement(yrc_parser_state_t* parser, yrc_ast_node_t** out, uint_fast8_t flags) {
  yrc_parser_symbol_t* sym = parser->symbol;
  yrc_ast_node_t* node;
  if (sym->std) {
    if (advance(parser, YRC_ISNT_REGEXP)) {
      return 1;
    }
    if (sym->std(parser, out, flags)) {
      return 1;
    }
    return 0;
  }
  node = yrc_pool_attain(parser->node_pool);
  *out = node;
  if (node == NULL) {
    return 1;
  }
  node->kind = YRC_AST_STMT_EXPR;
  if (commaexpression(parser, 0, &node->data.as_exprstmt.expression, flags)) {
    return 1;
  }

  if (!(flags & CONSUME_SEMICOLON)) {
    return 0;
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
    if (IS_OP(parser->token, RBRACE) ||
        IS_EOF(parser->token) ||
        IS_KW(parser->token, CASE) ||
        IS_KW(parser->token, DEFAULT)) {
      break;
    }
    stmt = NULL;
    if (IS_OP(parser->token, SEMICOLON)) {
      stmt = yrc_pool_attain(parser->node_pool);
      if (stmt == NULL) {
        return 1;
      }
      stmt->kind = YRC_AST_STMT_EMPTY;
      if (advance(parser, YRC_ISNT_REGEXP)) {
        return 1;
      }
    } else if (statement(parser, &stmt, CONSUME_SEMICOLON)) {
      return 1;
    }
    if (yrc_llist_push(out, stmt)) {
      return 1;
    }
  }
  return 0;
}

YRC_EXTERN int yrc_parse(yrc_parse_request_t* req, yrc_parse_response_t** out) {
  yrc_llist_t* stmts;
  yrc_parser_state_t parser = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    1,
    NULL
  };
  yrc_parse_response_priv_t* resp;
  resp = malloc(sizeof(*resp));
  resp->response.error = NULL;
  resp->response.root = NULL;
  parser.errorptr = &resp->response.error;
  parser.readcb = req->read;
  if (yrc_tokenizer_init(&parser.tokenizer, req->readsize, req->readctx)) {
    return 1;
  }

  if (yrc_pool_init(&parser.node_pool, sizeof(yrc_ast_node_t))) {
    yrc_tokenizer_free(parser.tokenizer);
    return 1;
  }
  resp->tokenizer = parser.tokenizer;
  resp->node_pool = parser.node_pool;

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

  resp->response.root = yrc_pool_attain(parser.node_pool);
  if (resp->response.root == NULL) {
    yrc_llist_free(stmts);
    yrc_tokenizer_free(parser.tokenizer);
    yrc_pool_free(parser.node_pool);
    return 1;
  }

  resp->response.root->kind = YRC_AST_PROGRAM;
  resp->response.root->data.as_program.body = stmts;
  *out = (yrc_parse_response_t*)resp;
  return 0;
}


static yrc_visitor_mode free_node(yrc_ast_node_t* node, yrc_rel rel, yrc_ast_node_t* parent, void* ctx);


YRC_EXTERN int yrc_parse_free(yrc_parse_response_t* resp_) {
  yrc_parse_response_priv_t* resp;
  yrc_visitor_t visitor;
  visitor.exit = free_node;
  visitor.enter = NULL;
  resp = (yrc_parse_response_priv_t*)resp_;

  yrc_traverse(resp->response.root, &visitor);
  yrc_tokenizer_free(resp->tokenizer);
  yrc_pool_free(resp->node_pool);
  free(resp_);
  return 0;
}

static yrc_visitor_mode free_node(yrc_ast_node_t* node, yrc_rel rel, yrc_ast_node_t* parent, void* ctx) {
  switch (node->kind) {
    default: return kYrcTraverseContinue;
    case YRC_AST_CLSE_CASE:
      yrc_llist_free(node->data.as_case.consequent);
    break;
    case YRC_AST_STMT_SWITCH:
      yrc_llist_free(node->data.as_switch.cases);
    break;
    case YRC_AST_EXPR_FUNCTION:
    case YRC_AST_DECL_FUNCTION:
      yrc_llist_free(node->data.as_function.params);
      yrc_llist_free(node->data.as_function.defaults);
    break;
    case YRC_AST_DECL_VAR:
      yrc_llist_free(node->data.as_var.declarations);
    break;
    case YRC_AST_PROGRAM:
    case YRC_AST_STMT_BLOCK:
      yrc_llist_free(node->data.as_block.body);
    break;
    case YRC_AST_EXPR_OBJECT:
      if (node->data.as_object.properties)
      yrc_llist_free(node->data.as_object.properties);
    break;
    case YRC_AST_EXPR_ARRAY:
      if (node->data.as_array.elements)
      yrc_llist_free(node->data.as_array.elements);
    break;
    case YRC_AST_EXPR_CALL:
      if (node->data.as_call.arguments)
      yrc_llist_free(node->data.as_call.arguments);
    break;
  }
  return kYrcTraverseContinue;
}
