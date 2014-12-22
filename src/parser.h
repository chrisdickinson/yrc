#ifndef _YRC_PARSER_H
#define _YRC_PARSER_H
#include "tokenizer.h"
typedef struct yrc_ast_node_s yrc_ast_node_t;
typedef struct yrc_parser_state_s yrc_parser_state_t;
typedef struct yrc_parser_symbol_s yrc_parser_symbol_t;

typedef int (*yrc_parser_led_t)(yrc_parser_state_t*, yrc_ast_node_t*, yrc_ast_node_t**);
typedef int (*yrc_parser_nud_t)(yrc_parser_state_t*, yrc_token_t*, yrc_ast_node_t**);
typedef int (*yrc_parser_std_t)(yrc_parser_state_t*, yrc_ast_node_t**);

typedef int64_t yrc_ast_node_type;
typedef int64_t yrc_property_type;

struct yrc_parser_symbol_s {
  yrc_parser_nud_t        nud;
  yrc_parser_led_t        led;
  yrc_parser_std_t        std;
  int64_t                 lbp;
};

#define AST_TYPE_MAP(XX) \
  XX(PROGRAM)\
  XX(STMT_EMPTY)\
  XX(STMT_EXPR)\
  XX(STMT_IF)\
  XX(STMT_LABEL)\
  XX(STMT_BREAK)\
  XX(STMT_CONTINUE)\
  XX(STMT_WITH)\
  XX(STMT_SWITCH)\
  XX(STMT_RETURN)\
  XX(STMT_THROW)\
  XX(STMT_TRY)\
  XX(STMT_WHILE)\
  XX(STMT_DOWHILE)\
  XX(STMT_FOR)\
  XX(STMT_FORIN)\
  XX(STMT_FOROF)\
  XX(DECL_FUNCTION)\
  XX(DECL_VAR)\
  XX(EXPR_THIS)\
  XX(EXPR_ARRAY)\
  XX(EXPR_OBJECT)\
  XX(EXPR_PROPERTY)\
  XX(EXPR_FUNCTION)\
  XX(EXPR_ARROW)\
  XX(EXPR_SEQUENCE)\
  XX(EXPR_UNARY)\
  XX(EXPR_BINARY)\
  XX(EXPR_ASSIGNMENT)\
  XX(EXPR_UPDATE)\
  XX(EXPR_LOGICAL)\
  XX(EXPR_CONDITIONAL)\
  XX(EXPR_NEW)\
  XX(EXPR_CALL)\
  XX(EXPR_MEMBER)\
  XX(EXPR_YIELD)

enum {
  YRC_AST_NULL=0,
#define XX(a) YRC_AST_##a,
  AST_TYPE_MAP(XX)
#undef XX
  YRC_AST_LAST
};

struct yrc_ast_node_s {
  yrc_ast_node_type kind;
  char rest[1];
};

typedef struct yrc_ast_node_return_s {
  yrc_ast_node_type kind;
  yrc_ast_node_t* argument;
} yrc_ast_node_return_t;

typedef struct yrc_ast_node_conditional_s {
  yrc_ast_node_type kind;
  yrc_ast_node_t* test;
  yrc_ast_node_t* consequent;
  yrc_ast_node_t* alternate;
} yrc_ast_node_conditional_t;

typedef struct yrc_ast_node_binary_s {
  yrc_ast_node_type kind;
  yrc_ast_node_t* left;
  yrc_ast_node_t* right;
  yrc_token_operator_t op;
} yrc_ast_node_binary_t;

typedef struct yrc_ast_node_unary_s {
  yrc_ast_node_type kind;
  yrc_ast_node_t* argument;
  yrc_token_operator_t op;
} yrc_ast_node_unary_t;

typedef struct yrc_ast_node_update_s {
  yrc_ast_node_type kind;
  yrc_ast_node_t* argument;
  yrc_token_operator_t op;
  int prefix;
} yrc_ast_node_update_t;

typedef struct yrc_ast_node_member_s {
  yrc_ast_node_type kind;
  yrc_ast_node_t* object;
  yrc_ast_node_t* property;
  int computed;
} yrc_ast_node_member_t;

typedef struct yrc_ast_node_call_s {
  yrc_ast_node_type kind;
  yrc_ast_node_t* callee;
  yrc_llist_t* arguments;
} yrc_ast_node_call_t;

typedef struct yrc_ast_node_array_s {
  yrc_ast_node_type kind;
  yrc_llist_t* elements;
} yrc_ast_node_array_t;

typedef struct yrc_ast_node_property_s {
  yrc_ast_node_type kind;
  yrc_ast_node_t* key;
  yrc_ast_node_t* expression;
  yrc_property_type type; /* normal / get / set / shorthand fn / computed */
} yrc_ast_node_property_t;

typedef struct yrc_ast_node_continue_s {
  yrc_ast_node_type kind;
  yrc_token_t* label;
} yrc_ast_node_continue_t;
typedef yrc_ast_node_continue_t yrc_ast_node_break_t;

typedef struct yrc_ast_node_exprstmt_s {
  yrc_ast_node_type kind;
  yrc_ast_node_t* expression;
} yrc_ast_node_exprstmt_t;

typedef struct yrc_ast_node_block_s {
  yrc_ast_node_type kind;
  yrc_llist_t* body;
} yrc_ast_node_block_t;
typedef yrc_ast_node_block_t yrc_ast_node_program_t;

typedef struct yrc_ast_node_ident_s {
  yrc_ast_node_type kind;
  yrc_token_t* name;
} yrc_ast_node_ident_t;

typedef struct yrc_ast_node_literal_s {
  yrc_ast_node_type kind;
  yrc_token_t* value;
} yrc_ast_node_literal_t;

#endif
