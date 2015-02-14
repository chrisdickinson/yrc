#ifndef YRC_STRING_H
#define YRC_STRING_H

typedef union {
    struct {
      size_t size;
      char*  data;
    } externed;
    struct {
      uint8_t externed;
      uint8_t data[sizeof(char*) + sizeof(size_t) - 1];
    } interned;
} yrc_str_t;

#endif
