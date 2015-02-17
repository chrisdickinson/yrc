#include "yrc-common.h"
#include "str.h"
#include <string.h>

enum {
  kInternedSize=sizeof(struct yrc_extern_str) - 1
};

inline static int is_interned(yrc_str_t* str) {
  return str->interned.flag & 0x1;
}

inline static int interned_size(yrc_str_t* str) {
  return str->interned.flag >> 1;
}

inline static int intern_cmp(yrc_str_t* lhs, yrc_str_t* rhs) {
  int i = 0;
  for (; i < kInternedSize; ++i) {
    if (lhs->interned.data[i] != rhs->interned.data[i]) {
      return 0;
    }
  }
  return 1;
}

int yrc_str_cmp(yrc_str_t* lhs, yrc_str_t* rhs) {
  size_t lhssz;
  size_t rhssz;
  int ret;
  lhssz = yrc_str_len(lhs);
  rhssz = yrc_str_len(rhs);
  ret = memcmp(yrc_str_ptr(lhs), yrc_str_ptr(rhs), lhssz < rhssz ? lhssz : rhssz);
  if (!ret) {
    return lhssz < rhssz ? -1 :
           lhssz > rhssz ? 1 : 0;
  }
  return ret;
}

size_t yrc_str_len(yrc_str_t* str) {
  if (!is_interned(str)) {
    return str->externed.size;
  }
  return interned_size(str);
}

static int externalize(yrc_str_t* str, char* data, size_t cur, size_t sz) {
  size_t toalloc;
  char* ptr;

  toalloc = npot(cur + sz);
  ptr = malloc(toalloc);
  if (ptr == NULL) {
    return 1;
  }

  memcpy(ptr, str->interned.data, cur);
  memcpy(ptr + cur, data, sz);
  str->externed.avail = toalloc;
  str->externed.size = cur + sz;
  str->externed.data = ptr;
  return 0;
}

static int do_interned_pushv(yrc_str_t* str, char* data, size_t sz) {
  size_t current;
  int i, j;
  int newsz;
  current = interned_size(str);
  newsz = current + sz;
  if (newsz >= kInternedSize) {
    return externalize(str, data, current, sz);
  }
  str->interned.flag = 1 | (newsz << 1);
  memcpy(str->interned.data, data, sz);
  return 0;
}

static int do_externed_pushv(struct yrc_extern_str* exstr, char* data, size_t sz) {
  size_t newsz;
  char* ptr;
  newsz = exstr->size + sz;
  if (newsz <= exstr->avail) {
    memcpy(exstr->data + exstr->size, data, sz);
    exstr->size = newsz;
    return 0;
  }
  newsz = npot(newsz);
  ptr = malloc(newsz);
  if (ptr == NULL) {
    return 1;
  }
  memcpy(ptr, exstr->data, exstr->size);
  memcpy(ptr + exstr->size, data, sz);
  exstr->avail = newsz;
  exstr->data = ptr;
  exstr->size = exstr->size + sz;
  return 0;
}

int yrc_str_push(yrc_str_t* str, char ch) {
  if (is_interned(str)) {
    return do_interned_pushv(str, &ch, 1);
  }
  return do_externed_pushv(&str->externed, &ch, 1);
}

int yrc_str_pushv(yrc_str_t* str, char* chs, size_t sz) {
  return 0;
}

char* yrc_str_ptr(yrc_str_t* str) {
  if (is_interned(str)) {
    return str->interned.data;
  }
  return str->externed.data;
}

int yrc_str_free(yrc_str_t* str) {
  if (!is_interned(str) && str->externed.data) {
    free(str->externed.data);
    return 0;
  }
}

int yrc_str_xfer(yrc_str_t* src, yrc_str_t* dst) {
  dst->externed.avail = src->externed.avail;
  dst->externed.data = src->externed.data;
  dst->externed.size = src->externed.size;
  if (!is_interned(src)) {
    if (src->externed.data == NULL) {
      UNREACHABLE();
    }
    src->externed.size = 0;
    src->externed.data = NULL;
  }
}
