#include "yrc-common.h"
#include "accumulator.h"
#include <stdlib.h> /* malloc + free */
#include <string.h> /* memcpy */

struct yrc_accum_s {
  size_t init;
  size_t size;
  size_t offs;
  char* head;
};


static size_t _npot(size_t in) {
  --in;
  in = in | (in >> 1);
  in = in | (in >> 2);
  in = in | (in >> 4);
  in = in | (in >> 8);
  in = in | (in >> 16);
  in = in | (in >> 32);
  ++in;

  return in;
}

static int _check_resize(yrc_accum_t* accum, size_t desired) {
  size_t new_size;
  char* new_head;
  if (accum->offs + desired <= accum->size) {
    return 0;
  }

  new_size = _npot(accum->offs + desired);
  new_head = realloc(accum->head, new_size);
  if (new_head == NULL) {
    return 1;
  }
  /* if successful, the old memory will be freed by realloc */
  accum->head = new_head;
  return 0;
}


int yrc_accum_init(yrc_accum_t** accum_p, size_t base_size) {
  yrc_accum_t* accum = malloc(sizeof(*accum));
  if (accum == NULL) {
    return 1;
  }
  accum->init = accum->size = base_size;
  accum->offs = 0;
  accum->head = malloc(base_size);
  if (accum->head == NULL) {
    free(accum);
    return 1;
  }
  *accum_p = accum;
  return 0;
}

int yrc_accum_push(yrc_accum_t* accum, char ch) {
  if (_check_resize(accum, 1)) {
    return 1;
  }
  accum->head[accum->offs++] = ch;
  return 0;
}

int yrc_accum_copy(yrc_accum_t* accum, char* data, size_t size) {
  if (_check_resize(accum, size)) {
    return 1;
  }

  memcpy(accum->head + accum->offs, data, size);
  accum->offs += size;
  return 0;
}

int yrc_accum_free(yrc_accum_t* accum) {
  free(accum->head);
  free(accum);
  return 0;
}

int yrc_accum_export(yrc_accum_t* accum, char** strptr, size_t* sizeptr) {
  *sizeptr = accum->offs;
  *strptr = accum->head;
  accum->head = malloc(accum->init);
  if (accum->head == NULL) {
    return 1;
  }
  accum->size = accum->init;
  accum->offs = 0;
  return 0;
}

int yrc_accum_borrow(yrc_accum_t* accum, char** strptr, size_t* sizeptr) {
  *sizeptr = accum->offs;
  *strptr = accum->head;
  return 0;
}

int yrc_accum_discard(yrc_accum_t* accum) {
  accum->offs = 0;
  return 0;
}
