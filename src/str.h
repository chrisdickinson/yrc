#ifndef _YRC_STR_H
#define _YRC_STR_H

int yrc_str_cmp(yrc_str_t*, yrc_str_t*);
int yrc_str_pushv(yrc_str_t*, const char*, size_t);
int yrc_str_push(yrc_str_t*, char);
void yrc_str_init(yrc_str_t*);
int yrc_str_free(yrc_str_t*);
int yrc_str_xfer(yrc_str_t*, yrc_str_t*);

#endif
