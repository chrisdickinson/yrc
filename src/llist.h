#ifndef _YRC_LLIST_H
#define _YRC_LLIST_H

#include "yrc-common.h"

typedef struct { void* ptr; } yrc_llist_iter_t;

int yrc_llist_init(yrc_llist_t**);
int yrc_llist_free(yrc_llist_t*);
int yrc_llist_push(yrc_llist_t*, void*);
void* yrc_llist_pop(yrc_llist_t*);
size_t yrc_llist_len(yrc_llist_t*);
void* yrc_llist_shift(yrc_llist_t*);
int yrc_llist_unshift(yrc_llist_t*, void*);

yrc_llist_iter_t yrc_llist_iter_start(yrc_llist_t*);
void* yrc_llist_iter_next(yrc_llist_iter_t*);

#endif
