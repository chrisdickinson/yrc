#ifndef _YRC_TOKENIZER_H
#define _YRC_TOKENIZER_H
#include "accumulator.h"


typedef enum {
  YRC_ISNT_REGEXP=0,
  YRC_IS_REGEXP=1,
  YRC_IS_REGEXP_EQ=2,
  YRC_NEXT_ADVANCE_FLAG=4
} yrc_scan_allow_regexp;

void yrc_token_repr(yrc_token_t*);
int yrc_tokenizer_init(yrc_tokenizer_t**, size_t, void*);
int yrc_tokenizer_scan(yrc_tokenizer_t*, yrc_readcb, yrc_token_t**, yrc_scan_allow_regexp);
int yrc_tokenizer_free(yrc_tokenizer_t*);
int yrc_tokenizer_eof(yrc_tokenizer_t*);
int yrc_tokenizer_promote_keyword(yrc_tokenizer_t*, yrc_token_t*);
#endif
