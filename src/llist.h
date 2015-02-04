#ifndef _YRC_LLIST_H
#define _YRC_LLIST_H

#include "yrc-common.h"

int yrc_llist_init(yrc_llist_t**);
int yrc_llist_free(yrc_llist_t*);
int yrc_llist_push(yrc_llist_t*, void*);
void* yrc_llist_pop(yrc_llist_t*);
size_t yrc_llist_len(yrc_llist_t*);
void* yrc_llist_shift(yrc_llist_t*);
int yrc_llist_unshift(yrc_llist_t*, void*);

#endif
