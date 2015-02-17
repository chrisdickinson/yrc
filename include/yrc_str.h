#ifndef YRC_STR_H
#define YRC_STR_H

typedef union yrc_str_u yrc_str_t;
YRC_EXTERN size_t yrc_str_len(yrc_str_t*);
YRC_EXTERN char* yrc_str_ptr(yrc_str_t*);

#endif
