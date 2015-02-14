#ifndef YRC_LLIST_H
#define YRC_LLIST_H

typedef struct yrc_llist_s yrc_llist_t;
typedef int (*yrc_llist_iter_cb_t)(void*, size_t, void*, int*);
typedef int (*yrc_llist_map_cb_t)(void*, void**, size_t, void*);
typedef int (*yrc_llist_filter_cb_t)(void*, int*, size_t, void*);
typedef int (*yrc_llist_reduce_cb_t)(void*, void*, void**, size_t, void*);
int yrc_llist_foreach(yrc_llist_t*, yrc_llist_iter_cb_t, void*);
int yrc_llist_map(yrc_llist_t*, yrc_llist_t*, yrc_llist_map_cb_t, void*);
int yrc_llist_filter(yrc_llist_t*, yrc_llist_t*, yrc_llist_filter_cb_t, void*);
int yrc_llist_reduce(yrc_llist_t*, void**, yrc_llist_reduce_cb_t, void*, void*);
int yrc_llist_any(yrc_llist_t*, int*, yrc_llist_filter_cb_t, void*);
int yrc_llist_all(yrc_llist_t*, int*, yrc_llist_filter_cb_t, void*);

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
