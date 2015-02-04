#include "yrc-common.h"
#include "llist.h"
#include <stdlib.h> /* malloc + free */
#include "pool.h"

typedef struct yrc_llist_node_s {
  void* item;
  struct yrc_llist_node_s* next;
} yrc_llist_node_t;


struct yrc_llist_s {
  yrc_llist_node_t* head;
  yrc_llist_node_t* tail;
  size_t size;
};


typedef struct _map_ctx_s {
  yrc_llist_map_cb_t inner_cb;
  yrc_llist_t* into;
  void* inner_ctx;
} _map_ctx_t;


typedef struct _filter_ctx_s {
  yrc_llist_filter_cb_t inner_cb;
  yrc_llist_t* into;
  void* inner_ctx;
} _filter_ctx_t;


typedef struct _reduce_ctx_s {
  yrc_llist_reduce_cb_t inner_cb;
  void* inner_ctx;
  void* last;
} _reduce_ctx_t;


typedef struct _anyall_ctx_s {
  yrc_llist_filter_cb_t inner_cb;
  void* inner_ctx;
  int output;
  int mode;  /* 0 = any, 1 = all */
} _anyall_ctx_t;

/* shared pool for all llist nodes */
static yrc_pool_t* llist_node_pool = NULL;

static void* alloc_node() {
  if (llist_node_pool == NULL) {
    yrc_pool_init(&llist_node_pool, sizeof(yrc_llist_node_t));
  }
  return yrc_pool_attain(llist_node_pool);
}

static int free_node(yrc_llist_node_t* node) {
  return yrc_pool_release(llist_node_pool, node);
}


int yrc_llist_init(yrc_llist_t** pllist) {
  yrc_llist_t* list;
  list = malloc(sizeof(*list));
  if (list == NULL) {
    return 1;
  }
  list->head = list->tail = NULL;
  list->size = 0;
  *pllist = list;
  return 0;
}


int yrc_llist_free(yrc_llist_t* list) {
  yrc_llist_node_t* current;
  yrc_llist_node_t* next;
  current = list->head;
  while(current) {
    next = current->next;
    free_node(current);
    current = next;
  }
  free(list);
  return 0;
}


int yrc_llist_push(yrc_llist_t* list, void* item) {
  yrc_llist_node_t* node;
  node = alloc_node();
  if (node == NULL) {
    return 1;
  }
  node->next = NULL;
  node->item = item;
  if (list->head == NULL) {
    list->head = list->tail = node;
    return 0;
  }
  list->tail->next = node;
  list->tail = node;
  ++list->size;
  return 0;
}


void* yrc_llist_pop(yrc_llist_t* list) {
  yrc_llist_node_t* tail;
  yrc_llist_node_t* iter;
  void* item;
  tail = list->tail;
  if (!tail) {
    return NULL;
  }
  iter = list->head;
  if (tail == iter) {
    item = iter->item;
    --list->size;
    free_node(iter);
    list->head = list->tail = NULL;
    return item;
  }
  while (iter && iter->next != list->tail) {
    iter = iter->next;
  }
  --list->size;
  item = iter->next->item;
  free_node(iter->next);
  iter->next = NULL;
  list->tail = iter;
  return item;
}


size_t yrc_llist_len(yrc_llist_t* list) {
  return list->size;
}


void* yrc_llist_shift(yrc_llist_t* list) {
  yrc_llist_node_t* next;
  void* item;
  if (!list->head) {
    return NULL;
  }
  if (list->head == list->tail) {
    item = list->head->item;
    --list->size;
    free_node(list->head);
    list->head = list->tail = NULL;
    return item;
  }
  next = list->head->next;
  item = list->head->item;
  free_node(list->head);
  list->head = next;
  --list->size;
  return item;
}


int yrc_llist_unshift(yrc_llist_t* list, void* item) {
  yrc_llist_node_t* node;
  node = alloc_node();
  if (node == NULL) {
    return 1;
  }
  node->item = item;
  node->next = list->head;
  list->head = node;
  if (!list->tail) {
    list->tail = node->next ? node->next : node;
  }
  ++list->size;
  return 0;
}


int yrc_llist_foreach(yrc_llist_t* list, yrc_llist_iter_cb_t cb, void* ctx) {
  yrc_llist_node_t* current;
  size_t idx;
  int stop;

  stop = 0;
  idx = 0;
  current = list->head;
  while(current) {
    if (cb(current->item, idx, ctx, &stop)) {
      return 1;
    }
    if (stop) {
      break;
    }
    ++idx;
    current = current->next;
  }
  return 0;
}


int _llist_map_cb(void* item, size_t idx, void* outer_ctx, int* stop) {
  _map_ctx_t* mapctx;
  void* output;
  int r;
  mapctx = (_map_ctx_t*)outer_ctx;
  r = mapctx->inner_cb(item, &output, idx, mapctx->inner_ctx);
  if (r) {
    return r;
  }
  r = yrc_llist_push(mapctx->into, output);
  if (r) {
    return r;
  }
  return 0;
}


int _llist_filter_cb(void* item, size_t idx, void* outer_ctx, int* stop) {
  _filter_ctx_t* filtctx;
  int output;
  int r;
  filtctx = (_filter_ctx_t*)outer_ctx;
  r = filtctx->inner_cb(item, &output, idx, filtctx->inner_ctx);
  if (r) {
    return r;
  }
  if (output == 0) {
    return 0;
  }
  r = yrc_llist_push(filtctx->into, item);
  if (r) {
    return r;
  }
  return 0;
}


int _llist_reduce_cb(void* item, size_t idx, void* outer_ctx, int* stop) {
  _reduce_ctx_t* redctx;
  void* output;
  int r;

  redctx = (_reduce_ctx_t*)outer_ctx;
  r = redctx->inner_cb(redctx->last, item, &output, idx, redctx->inner_ctx);
  if (r) {
    return r;
  }
  redctx->last = output;
  return 0;
}


int _llist_anyall_cb(void* item, size_t idx, void* ctx, int* stop) {
  _anyall_ctx_t* outer_ctx;
  int r;
  outer_ctx = (_anyall_ctx_t*)ctx;
  r = outer_ctx->inner_cb(item, &outer_ctx->output, idx, outer_ctx->inner_ctx);
  if (r) {
    return r;
  }
  if (outer_ctx->output != outer_ctx->mode) {
    *stop = 1;
  }
  return 0;
}


int yrc_llist_map(yrc_llist_t* from, yrc_llist_t* into, yrc_llist_map_cb_t cb, void* ctx) {
  _map_ctx_t outer_ctx;
  outer_ctx.inner_cb = cb;
  outer_ctx.into = into;
  outer_ctx.inner_ctx = ctx;
  if (yrc_llist_foreach(from, _llist_map_cb, &outer_ctx)) {
    return 1;
  }
  return 0;
}


int yrc_llist_filter(yrc_llist_t* from, yrc_llist_t* into, yrc_llist_filter_cb_t cb, void* ctx) {
  _filter_ctx_t outer_ctx;
  outer_ctx.inner_cb = cb;
  outer_ctx.into = into;
  outer_ctx.inner_ctx = ctx;
  if (yrc_llist_foreach(from, _llist_filter_cb, &outer_ctx)) {
    return 1;
  }
  return 0;
}


int yrc_llist_reduce(yrc_llist_t* from, void** into, yrc_llist_reduce_cb_t cb, void* init, void* ctx) {
  _reduce_ctx_t outer_ctx;
  outer_ctx.inner_cb = cb;
  outer_ctx.inner_ctx = ctx;
  outer_ctx.last = init;
  if (yrc_llist_foreach(from, _llist_reduce_cb, &outer_ctx)) {
    return 1;
  }
  *into = outer_ctx.last;
  return 0;
}


int _yrc_llist_anyall(yrc_llist_t* from, int* result, yrc_llist_filter_cb_t cb, void* ctx, int mode) {
  _anyall_ctx_t outer_ctx;
  outer_ctx.inner_cb = cb;
  outer_ctx.inner_ctx = ctx;
  outer_ctx.mode = mode;
  if (yrc_llist_foreach(from, _llist_anyall_cb, &outer_ctx)) {
    return 1;
  }
  *result = outer_ctx.mode == outer_ctx.output;
  return 0;
}


int yrc_llist_any(yrc_llist_t* from, int* result, yrc_llist_filter_cb_t cb, void* ctx) {
  return _yrc_llist_anyall(from, result, cb, ctx, 0);
}


int yrc_llist_all(yrc_llist_t* from, int* result, yrc_llist_filter_cb_t cb, void* ctx) {
  return _yrc_llist_anyall(from, result, cb, ctx, 1);
}
