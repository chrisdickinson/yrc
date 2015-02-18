#ifndef YRC_STR_H
#define YRC_STR_H

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

typedef union yrc_str_u {
  struct yrc_extern_str externed;
  struct {
    char flag;
    char data[1];
  } interned;
} yrc_str_t;

YRC_EXTERN size_t yrc_str_len(yrc_str_t*);
YRC_EXTERN char* yrc_str_ptr(yrc_str_t*);

#endif
