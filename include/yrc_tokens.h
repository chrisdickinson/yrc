#ifndef YRC_TOKENS_H
#define YRC_TOKENS_H
#include "yrc_str.h"
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
  yrc_str_t str;
  yrc_regexp_flags flags;
} yrc_token_regexp_t;

typedef struct yrc_token_string_s {
  yrc_token_string_delim delim;  /* " ' ''' """ */
  yrc_str_t str;
} yrc_token_string_t;

typedef struct yrc_token_number_s {
  yrc_token_number_repr repr; /* 0xXX .dd d.d ded dEd .ddEd .dded d.dEd d.ded */
  union {
    uint64_t as_int;
    double as_double;
  } data;
} yrc_token_number_t;

typedef struct yrc_token_ident_s {
  yrc_str_t str;
} yrc_token_ident_t;

typedef struct yrc_token_comment_s {
  yrc_token_comment_delim delim; /* c-style cpp-style */
  yrc_str_t str;
} yrc_token_comment_t;

typedef struct yrc_token_whitespace_s {
  yrc_str_t str;
  int has_newline;
} yrc_token_whitespace_t;

typedef struct yrc_position_s {
  size_t line;
  size_t col;
  size_t fpos;
} yrc_position_t;

/* XXX: use a tree structure for file positions so we don't have
   an 80 byte token type */
typedef struct yrc_token_s {
  yrc_token_type type;
  union {
#define XX(a, b) yrc_token_##b##_t as_##b;
  YRC_TOKEN_TYPES(XX)
#undef XX
  } info;
  yrc_position_t start;
  yrc_position_t end;
} yrc_token_t;

typedef struct yrc_tokenizer_s yrc_tokenizer_t;
#endif
