#ifndef _YRC_TOKENIZER_H
#define _YRC_TOKENIZER_H
#include "accumulator.h"

#define YRC_TOKENIZER_STATE_MAP(XX) \
  XX(DEFAULT, default) \
  XX(WHITESPACE, whitespace) \
  XX(STRING, string) \
  XX(STRING_ESCAPE, string_escape) \
  XX(STRING_UNICODE, string_unicode) \
  XX(STRING_HEX, string_hex) \
  XX(NUMBER, number) \
  XX(IDENTIFIER, identifier) \
  XX(OPERATOR, operator) \
  XX(COMMENT_LINE, comment_line) \
  XX(COMMENT_BLOCK, comment_block)

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

enum {
  YRC_TKS_NULL=0,
  YRC_TKS_ERROR,
  YRC_TKS_DONE
#define XX(a, b) , YRC_TKS_##a
  YRC_TOKENIZER_STATE_MAP(XX)
#undef XX
};

typedef struct yrc_op_s yrc_op_t;

typedef uint32_t yrc_tokenizer_state;

struct yrc_tokenizer_s {
  yrc_tokenizer_state state;
  yrc_token_comment_delim comment_delim;
  yrc_token_string_delim string_delim;
  yrc_llist_t* tokens;
  uint32_t seen_flags;  /* 0x01 == dot, 0x10 == exponent e, 0x100 == exponent E, 0x1000 == x */
  uint32_t last_char;
  yrc_accum_t* accum_primary;
  yrc_accum_t* accum_secondary;
  yrc_op_t* op_current;
  yrc_op_t* op_last;

  uint64_t last_fpos;
  uint64_t last_line;
  uint64_t last_col;

  uint64_t fpos;
  uint64_t line;
  uint64_t col;
};
#endif
