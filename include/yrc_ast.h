#ifndef YRC_AST_H
#define YRC_AST_H
#include "yrc_llist.h"
#include "yrc_tokens.h"

#define YRC_AST_TYPE_MAP(XX) \
  XX(PROGRAM)\
  XX(STMT_BLOCK)\
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
  XX(CLSE_CASE)\
  XX(CLSE_CATCH)\
  XX(CLSE_VAR)\
  XX(STMT_WHILE)\
  XX(STMT_DOWHILE)\
  XX(STMT_FOR)\
  XX(STMT_FORIN)\
  XX(STMT_FOROF)\
  XX(DECL_FUNCTION)\
  XX(DECL_VAR)\
  XX(EXPR_IDENTIFIER)\
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
  XX(EXPR_LITERAL)\
  XX(EXPR_LOGICAL)\
  XX(EXPR_CONDITIONAL)\
  XX(EXPR_NEW)\
  XX(EXPR_CALL)\
  XX(EXPR_MEMBER)\
  XX(EXPR_YIELD)

typedef enum {
  YRC_OK,
  YRC_EUNEXPECTED,
  YRC_ENOTALLOWED,
  YRC_EBADTOKEN,
  YRC_EMEM
} yrc_parse_error_type;
typedef struct yrc_error_s yrc_error_t;

typedef enum {
  YRC_AST_NULL=0,
#define XX(a) YRC_AST_##a,
  YRC_AST_TYPE_MAP(XX)
#undef XX
  YRC_AST_LAST
} yrc_ast_node_type;

typedef enum {
  REL_NONE,
  REL_ALTERNATE,
  REL_ARGUMENT,
  REL_ARGUMENTS,
  REL_BLOCK,
  REL_BODY,
  REL_CALLEE,
  REL_CASES,
  REL_CONSEQUENT,
  REL_DECLARATIONS,
  REL_DEFAULTS,
  REL_DISCRIMINANT,
  REL_ELEMENTS,
  REL_EXPRESSION,
  REL_FINALIZER,
  REL_HANDLER,
  REL_ID,
  REL_INIT,
  REL_KEY,
  REL_LEFT,
  REL_OBJECT,
  REL_PARAM,
  REL_PARAMS,
  REL_PROPERTIES,
  REL_PROPERTY,
  REL_RIGHT,
  REL_TEST,
  REL_UPDATE
} yrc_rel;


typedef struct yrc_ast_node_s yrc_ast_node_t;
typedef struct yrc_parser_state_s yrc_parser_state_t;

typedef enum {
  YRC_PROP_SPC=1,
  YRC_PROP_GET=2,
  YRC_PROP_SET=4,
  YRC_PROP_COMPUTED=8,
  YRC_PROP_SHORTHAND=16,
  YRC_PROP_SHORTHAND_METHOD=32
} yrc_property_flags;

typedef enum {
  YRC_VARTYPE_VAR=0,
  YRC_VARTYPE_CONST=1,
  YRC_VARTYPE_LET=2
} yrc_var_type;

typedef struct yrc_ast_node_return_s {
  yrc_ast_node_t* argument;
} yrc_ast_node_return_t;

typedef yrc_ast_node_return_t yrc_ast_node_throw_t;

typedef struct yrc_ast_node_conditional_s {
  yrc_ast_node_t* test;
  yrc_ast_node_t* consequent;
  yrc_ast_node_t* alternate;
} yrc_ast_node_conditional_t;

typedef yrc_ast_node_conditional_t yrc_ast_node_if_t;

typedef struct yrc_ast_node_binary_s {
  yrc_ast_node_t* left;
  yrc_ast_node_t* right;
  yrc_operator_t op;
} yrc_ast_node_binary_t;

typedef yrc_ast_node_binary_t yrc_ast_node_assign_t;

typedef struct yrc_ast_node_unary_s {
  yrc_ast_node_t* argument;
  yrc_operator_t op;
} yrc_ast_node_unary_t;

typedef struct yrc_ast_node_update_s {
  yrc_ast_node_t* argument;
  yrc_operator_t op;
  int prefix;
} yrc_ast_node_update_t;

typedef struct yrc_ast_node_member_s {
  yrc_ast_node_t* object;
  yrc_ast_node_t* property;
  int computed;
} yrc_ast_node_member_t;

typedef struct yrc_ast_node_call_s {
  yrc_ast_node_t* callee;
  yrc_llist_t* arguments;
} yrc_ast_node_call_t;

typedef struct yrc_ast_node_array_s {
  yrc_llist_t* elements;
} yrc_ast_node_array_t;

typedef struct yrc_ast_node_object_s {
  yrc_llist_t* properties;
} yrc_ast_node_object_t;

typedef struct yrc_ast_node_property_s {
  yrc_ast_node_t* key;
  yrc_ast_node_t* expression;
  yrc_property_flags type; /* normal / get / set / shorthand fn / computed */
} yrc_ast_node_property_t;

typedef struct yrc_ast_node_continue_s {
  yrc_token_t* label;
} yrc_ast_node_continue_t;
typedef yrc_ast_node_continue_t yrc_ast_node_break_t;

typedef struct yrc_ast_node_exprstmt_s {
  yrc_ast_node_t* expression;
} yrc_ast_node_exprstmt_t;

typedef struct yrc_ast_node_block_s {
  yrc_llist_t* body;
} yrc_ast_node_block_t;
typedef yrc_ast_node_block_t yrc_ast_node_program_t;

typedef struct yrc_ast_node_ident_s {
  yrc_token_t* name;
} yrc_ast_node_ident_t;

typedef struct yrc_ast_node_literal_s {
  yrc_token_t* value;
} yrc_ast_node_literal_t;

typedef struct yrc_ast_node_while_s {
  yrc_ast_node_t* test;
  yrc_ast_node_t* body;
} yrc_ast_node_while_t;
typedef yrc_ast_node_while_t yrc_ast_node_do_while_t;

typedef struct yrc_ast_node_catch_s {
  yrc_ast_node_t* param;
  yrc_ast_node_t* body;
} yrc_ast_node_catch_t;

typedef struct yrc_ast_node_try_s {
  yrc_ast_node_t* block;
  yrc_ast_node_t* handler;
  yrc_ast_node_t* finalizer;
} yrc_ast_node_try_t;

typedef struct yrc_ast_node_var_s {
  yrc_llist_t* declarations;
  yrc_var_type type;
} yrc_ast_node_var_t;

typedef struct yrc_ast_node_vardecl_s {
  yrc_ast_node_t* id;
  yrc_ast_node_t* init;
} yrc_ast_node_vardecl_t;

typedef struct yrc_ast_node_sequence_s {
  yrc_ast_node_t* left;
  yrc_ast_node_t* right;
} yrc_ast_node_sequence_t;

typedef struct yrc_ast_node_function_s {
  yrc_token_t* id;
  yrc_llist_t* params;
  yrc_llist_t* defaults;
  yrc_ast_node_t* body;
} yrc_ast_node_function_t;

typedef struct yrc_ast_node_for_s {
  yrc_ast_node_t* init;
  yrc_ast_node_t* test;
  yrc_ast_node_t* update;
  yrc_ast_node_t* body;
} yrc_ast_node_for_t;

typedef struct yrc_ast_node_for_in_s {
  yrc_ast_node_t* left;
  yrc_ast_node_t* right;
  yrc_ast_node_t* body;
} yrc_ast_node_for_in_t;


typedef struct yrc_ast_node_case_s {
  yrc_ast_node_t* test;
  yrc_llist_t* consequent;
} yrc_ast_node_case_t;


typedef struct yrc_ast_node_switch_s {
  yrc_ast_node_t* discriminant;
  yrc_llist_t* cases;
} yrc_ast_node_switch_t;

typedef yrc_ast_node_for_in_t yrc_ast_node_for_of_t;

struct yrc_ast_node_s {
  yrc_ast_node_type kind;
  uint_fast8_t has_parens;
  union {
    yrc_ast_node_array_t        as_array;
    yrc_ast_node_assign_t       as_assign;
    yrc_ast_node_binary_t       as_binary;
    yrc_ast_node_block_t        as_block;
    yrc_ast_node_break_t        as_break;
    yrc_ast_node_call_t         as_call;
    yrc_ast_node_case_t         as_case;
    yrc_ast_node_catch_t        as_catch;
    yrc_ast_node_conditional_t  as_conditional;
    yrc_ast_node_continue_t     as_continue;
    yrc_ast_node_exprstmt_t     as_exprstmt;
    yrc_ast_node_for_t          as_for;
    yrc_ast_node_for_in_t       as_for_in;
    yrc_ast_node_for_of_t       as_for_of;
    yrc_ast_node_function_t     as_function;
    yrc_ast_node_ident_t        as_ident;
    yrc_ast_node_if_t           as_if;
    yrc_ast_node_literal_t      as_literal;
    yrc_ast_node_member_t       as_member;
    yrc_ast_node_object_t       as_object;
    yrc_ast_node_program_t      as_program;
    yrc_ast_node_property_t     as_property;
    yrc_ast_node_return_t       as_return;
    yrc_ast_node_sequence_t     as_sequence;
    yrc_ast_node_switch_t       as_switch;
    yrc_ast_node_throw_t        as_throw;
    yrc_ast_node_try_t          as_try;
    yrc_ast_node_unary_t        as_unary;
    yrc_ast_node_update_t       as_update;
    yrc_ast_node_var_t          as_var;
    yrc_ast_node_vardecl_t      as_vardecl;
    yrc_ast_node_while_t        as_while;
    yrc_ast_node_do_while_t     as_do_while;
  } data;
};

#endif
