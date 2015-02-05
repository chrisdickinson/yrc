#ifndef YRC_H
#define YRC_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#if defined(_MSC_VER) && _MSC_VER < 1600
# include "stdint-msvc2008.h"
#else
# include <stdint.h>
#endif

#ifdef _WIN32
  /* Windows - set up dll import/export decorators. */
# if defined(BUILDING_YRC_SHARED)
    /* Building shared library. */
#   define YRC_EXTERN __declspec(dllexport)
# elif defined(USING_YRC_SHARED)
    /* Using shared library. */
#   define YRC_EXTERN __declspec(dllimport)
# else
    /* Building static library. */
#   define YRC_EXTERN /* nothing */
# endif
#elif __GNUC__ >= 4
# define YRC_EXTERN __attribute__((visibility("default")))
#else
# define YRC_EXTERN /* nothing */
#endif

#ifndef FPOS_T
#define FPOS_T uint64_t
#endif

typedef struct yrc_llist_s yrc_llist_t;
typedef int (*yrc_llist_iter_cb_t)(void*, size_t, void*, int*);
typedef int (*yrc_llist_map_cb_t)(void*, void**, size_t, void*);
typedef int (*yrc_llist_filter_cb_t)(void*, int*, size_t, void*);
typedef int (*yrc_llist_reduce_cb_t)(void*, void*, void**, size_t, void*);
int yrc_llist_foreach(yrc_llist_t*, yrc_llist_iter_cb_t, void*);
int yrc_llist_map(yrc_llist_t*, yrc_llist_t*, yrc_llist_map_cb_t, void*);
int yrc_llist_filter(yrc_llist_t*, yrc_llist_t*, yrc_llist_filter_cb_t, void*);
int yrc_llist_reduce(yrc_llist_t*, void**, yrc_llist_reduce_cb_t, void*, void*);
int yrc_llist_any(yrc_llist_t*, int*, yrc_llist_filter_cb_t, void*);
int yrc_llist_all(yrc_llist_t*, int*, yrc_llist_filter_cb_t, void*);


/**
  token types -- in "ENUM" and "ident"
  flavors.
**/
#define YRC_TOKEN_TYPES(XX) \
  XX(STRING, string) \
  XX(NUMBER, number) \
  XX(REGEXP, regexp) \
  XX(IDENT, ident) \
  XX(KEYWORD, keyword) \
  XX(OPERATOR, operator) \
  XX(COMMENT, comment) \
  XX(WHITESPACE, whitespace)

/**
  keywords -- from string to ENUM.
**/
#define YRC_KEYWORD_MAP(XX) \
  XX("break", BREAK)\
  XX("case", CASE)\
  XX("catch", CATCH)\
  XX("class", CLASS)\
  XX("const", CONST)\
  XX("continue", CONTINUE)\
  XX("debugger", DEBUGGER)\
  XX("default", DEFAULT)\
  XX("delete", DELETE)\
  XX("do", DO)\
  XX("else", ELSE)\
  XX("export", EXPORT)\
  XX("extends", EXTENDS)\
  XX("finally", FINALLY)\
  XX("for", FOR)\
  XX("function", FUNCTION)\
  XX("if", IF)\
  XX("import", IMPORT)\
  XX("in", IN)\
  XX("instanceof", INSTANCEOF)\
  XX("let", LET)\
  XX("of", OF)\
  XX("new", NEW)\
  XX("return", RETURN)\
  XX("super", SUPER)\
  XX("switch", SWITCH)\
  XX("this", THIS)\
  XX("throw", THROW)\
  XX("try", TRY)\
  XX("typeof", TYPEOF)\
  XX("var", VAR)\
  XX("void", VOID)\
  XX("while", WHILE)\
  XX("with", WITH)\
  XX("yield", YIELD)


/**
  operators -- from string to ENUM.
**/
#define YRC_OPERATOR_MAP(XX) \
  XX("=", EQ)\
  XX("==", EQEQ)\
  XX("===", EQEQEQ)\
  XX("+", ADD)\
  XX("+=", ADDEQ)\
  XX("++", INCR)\
  XX("-", SUB)\
  XX("-=", SUBEQ)\
  XX("--", DECR)\
  XX("!", NOT)\
  XX("!=", NOTEQ)\
  XX("!==", NOTEQEQ)\
  XX("&", AND)\
  XX("&=", ANDEQ)\
  XX("&&", LAND)\
  XX("|", OR)\
  XX("|=", OREQ)\
  XX("||", LOR)\
  XX("^", XOR)\
  XX("^=", XOREQ)\
  XX("^^", LXOR)\
  XX("%", MOD)\
  XX("%=", MODEQ)\
  XX("*", MUL)\
  XX("*=", MULEQ)\
  XX("/", DIV)\
  XX("/=", DIVEQ)\
  XX("(", LPAREN)\
  XX(")", RPAREN)\
  XX(".", DOT)\
  XX(",", COMMA)\
  XX("?", QUESTION)\
  XX("~", TILDE)\
  XX(":", COLON)\
  XX(";", SEMICOLON)\
  XX("{", LBRACE)\
  XX("}", RBRACE)\
  XX("[", LBRACK)\
  XX("]", RBRACK)\
  XX("<", LESSER)\
  XX("<=", LESSEREQ)\
  XX("<<", LSHF)\
  XX("<<=", LSHFEQ)\
  XX(">", GREATER)\
  XX(">=", GREATEREQ)\
  XX(">>", RSHF)\
  XX(">>=", RSHFEQ)\
  XX(">>>", URSHF)\
  XX(">>>=", URSHFEQ)

typedef enum {
  YRC_OP_NULL=0,
#define XX(a, b) YRC_OP_##b,
  YRC_OPERATOR_MAP(XX)
#undef XX
  YRC_OP_LAST
} yrc_operator_t;
typedef yrc_operator_t yrc_token_operator_t;

typedef enum {
  YRC_KWOP_FENCE=YRC_OP_LAST,
#define XX(a, b) YRC_KW_##b,
  YRC_KEYWORD_MAP(XX)
#undef XX
  YRC_OP_FINAL
} yrc_keyword_t;
typedef yrc_keyword_t yrc_token_keyword_t;

typedef enum {
  #define XX(a, b) YRC_TOKEN_##a,
  YRC_TOKEN_TYPES(XX)
  #undef XX
  YRC_TOKEN_EOF
} yrc_token_type;

typedef enum {
  YRC_COMMENT_DELIM_NONE=0,
  YRC_COMMENT_DELIM_LINE,
  YRC_COMMENT_DELIM_BLOCK
} yrc_token_comment_delim;

typedef enum {
  YRC_STRING_DELIM_NONE=0,
  YRC_STRING_DELIM_SINGLE,
  YRC_STRING_DELIM_DOUBLE
} yrc_token_string_delim;

typedef enum {
  REPR_SEEN_DOT=0x0001,
  REPR_SEEN_HEX=0x0002,
  REPR_SEEN_EXP_LE=0x0004,
  REPR_SEEN_EXP_BE=0x0008,
  REPR_SEEN_ANY=0x000F,
  REPR_IS_FLOAT=0x0010,
  REPR_SEEN_EXP=0x000c
} yrc_token_number_repr;

typedef enum {
  YRC_REGEXP_GLOBAL=0x1,
  YRC_REGEXP_IGNORECASE=0x2,
  YRC_REGEXP_MULTILINE=0x4,
  YRC_REGEXP_STICKY=0x8
} yrc_regexp_flags;

typedef struct yrc_token_regexp_s {
  size_t size;
  char* data;
  yrc_regexp_flags flags;
} yrc_token_regexp_t;

typedef struct yrc_token_string_s {
  yrc_token_string_delim delim;  /* " ' ''' """ */
  size_t size;
  char* data;
} yrc_token_string_t;

typedef struct yrc_token_number_s {
  yrc_token_number_repr repr; /* 0xXX .dd d.d ded dEd .ddEd .dded d.dEd d.ded */
  union {
    uint64_t as_int;
    double as_double;
  } data;
} yrc_token_number_t;

typedef struct yrc_token_ident_s {
  size_t size;
  char* data;
} yrc_token_ident_t;

typedef struct yrc_token_comment_s {
  yrc_token_comment_delim delim; /* c-style cpp-style */
  size_t size;
  char* data;
} yrc_token_comment_t;

typedef struct yrc_token_whitespace_s {
  size_t size;
  char* data;
  int has_newline;
} yrc_token_whitespace_t;

typedef struct yrc_position_s {
  FPOS_T line;
  FPOS_T col;
  FPOS_T fpos;
} yrc_position_t;

typedef struct yrc_token_s {
  yrc_token_type type;
  yrc_position_t start;
  yrc_position_t end;

  union {
#define XX(a, b) yrc_token_##b##_t as_##b;
  YRC_TOKEN_TYPES(XX)
#undef XX
  } info;
} yrc_token_t;

typedef struct yrc_tokenizer_s yrc_tokenizer_t;


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
  REL_CONSEQUENT,
  REL_DECLARATIONS,
  REL_DEFAULTS,
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

typedef int64_t yrc_property_type;

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
  yrc_property_type type; /* normal / get / set / shorthand fn / computed */
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


typedef size_t (*yrc_readcb)(char*, size_t, void*);
typedef struct yrc_parse_request_s {
  yrc_readcb      read;
  size_t          readsize;
  void*           readctx;
} yrc_parse_request_t;

typedef struct yrc_parse_response_s {
  yrc_ast_node_t* root;
  yrc_error_t*    error;
} yrc_parse_response_t;

YRC_EXTERN int yrc_parse(yrc_parse_request_t*, yrc_parse_response_t**);
YRC_EXTERN int yrc_parse_free(yrc_parse_response_t*);
YRC_EXTERN int yrc_error(yrc_error_t*, char*, size_t);
YRC_EXTERN int yrc_error_token(yrc_error_t*, const char**);
YRC_EXTERN int yrc_error_position(yrc_error_t*, FPOS_T*, FPOS_T*, FPOS_T*);

#ifdef __cplusplus
}
#endif
#endif
