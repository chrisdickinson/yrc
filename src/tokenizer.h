#ifndef _YRC_TOKENIZER_H
#define _YRC_TOKENIZER_H
#include "accumulator.h"


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
  XX("while", WHILE)\
  XX("do", DO)\
  XX("if", IF)\
  XX("for", FOR)\
  XX("else", ELSE)\
  XX("break", BREAK)\
  XX("continue", CONTINUE)\
  XX("return", RETURN)\
  XX("try", TRY)\
  XX("var", VAR)\
  XX("in", IN)\
  XX("instanceof", INSTANCEOF)\
  XX("void", VOID)\
  XX("typeof", TYPEOF)\
  XX("delete", DELETE)\
  XX("new", NEW)


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

enum {
  #define XX(a, b) YRC_TOKEN_##a,
  YRC_TOKEN_TYPES(XX)
  #undef XX
  YRC_TOKEN_EOF
};

enum {
  YRC_OP_NULL=0,
#define XX(a, b) YRC_OP_##b,
  YRC_OPERATOR_MAP(XX)
#undef XX
  YRC_KWOP_FENCE,
#define XX(a, b) YRC_KW_##b,
  YRC_KEYWORD_MAP(XX)
#undef XX
  YRC_OP_FINAL
};

enum {
  YRC_COMMENT_DELIM_NONE=0,
  YRC_COMMENT_DELIM_LINE,
  YRC_COMMENT_DELIM_BLOCK
};

enum {
  YRC_STRING_DELIM_NONE=0,
  YRC_STRING_DELIM_SINGLE,
  YRC_STRING_DELIM_DOUBLE
};


typedef uint32_t yrc_token_comment_delim;
typedef uint32_t yrc_token_string_delim;
typedef uint32_t yrc_token_number_repr;
typedef uint32_t yrc_token_type;

#define REPR_SEEN_DOT    0x0001
#define REPR_SEEN_HEX    0x0002
#define REPR_SEEN_EXP_LE 0x0004
#define REPR_SEEN_EXP_BE 0x0008
#define REPR_SEEN_EXP    (REPR_SEEN_EXP_LE | REPR_SEEN_EXP_BE)
#define REPR_SEEN_ANY    0x000F
#define REPR_IS_FLOAT    0x0010
typedef uint_fast8_t yrc_token_operator_t;
typedef uint_fast8_t yrc_token_keyword_t;
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
  uint64_t line;
  uint64_t col;
  uint64_t fpos;
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

enum yrc_scan_allow_regexp {
  YRC_ISNT_REGEXP=0,
  YRC_IS_REGEXP,
  YRC_IS_REGEXP_EQ
};

void yrc_token_repr(yrc_token_t*);
int yrc_tokenizer_init(yrc_tokenizer_t**, size_t);
int yrc_tokenizer_scan(yrc_tokenizer_t*, yrc_readcb, yrc_token_t**, enum yrc_scan_allow_regexp);
int yrc_tokenizer_free(yrc_tokenizer_t*);
int yrc_tokenizer_eof(yrc_tokenizer_t*);
#endif
