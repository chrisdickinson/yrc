#ifndef _YRC_STR_H
#define _YRC_STR_H

/*
  assumptions:
    8-byte alignment on data pointer
      use LSB byte (little-endian) to flag whether
      or not the string is interned
*/

struct yrc_extern_str {
  char*  data;  /* data *must* come first! */
  size_t size;
  size_t avail;
};


union yrc_str_u {
  struct yrc_extern_str externed;
  struct {
    char flag;
    char data[];
  } interned;
};


int yrc_str_cmp(yrc_str_t*, yrc_str_t*);
int yrc_str_pushv(yrc_str_t*, char*, size_t);
int yrc_str_push(yrc_str_t*, char);
int yrc_str_free(yrc_str_t*);
int yrc_str_xfer(yrc_str_t*, yrc_str_t*);

#endif
