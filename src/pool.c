#include "yrc-common.h"
#include "pool.h"
#include <stdlib.h> /* malloc + free */
#include <stdio.h>

#define clz(xs) __builtin_clzll(xs)
/**

  +------------+------------+---------------------------------------------+
  | next       | used       | data                                        |
  |            |            +----------+-----------+----------+-----------+
  |            |            | arenaptr | obj....   | arenaptr | obj...    |
  +------------+------------+----------+-----------+----------+-----------+
  used "1" bits indicate "free space", "0" bits indicate "taken"
  alloc = count leading zeros 

**/

typedef struct yrc_pool_arena_s {
  struct yrc_pool_arena_s* next;
  size_t used_mask;
  uint_fast16_t free;
  char data[1];
} yrc_pool_arena_t;

struct yrc_pool_s {
  yrc_pool_arena_t* head;
  yrc_pool_arena_t* current;
  yrc_pool_arena_t* last;
  size_t objsize;
  size_t num_arenas;
  uint_fast8_t deallocs;
};

static yrc_pool_arena_t* alloc_arena(yrc_pool_t* pool) {
  yrc_pool_arena_t** iter;
  yrc_pool_arena_t* arena;
  uint_fast8_t i;
  arena = malloc(sizeof(yrc_pool_arena_t) - 1 + (sizeof(yrc_pool_arena_t**) + pool->objsize) * 64);
  if (arena == NULL) {
    return NULL;
  }
  arena->used_mask = 0;
  arena->next = NULL;
  arena->used_mask = 0xFFFFFFFFFFFFFFFF;
  arena->free = 64;
  ++pool->num_arenas;
  iter = (yrc_pool_arena_t**)arena->data;
  for(i = 0; i < 64; ++i) {
    *iter = arena;
    iter = (void*)((sizeof(yrc_pool_arena_t**) + pool->objsize) + (size_t)(iter));
  }
  return arena;
}

int yrc_pool_init(yrc_pool_t** ptr, size_t objsize) {
  yrc_pool_t* pool = malloc(sizeof(*pool));
  if (pool == NULL) {
    return 1;
  }
  pool->objsize = objsize;
  pool->num_arenas = 0;
  pool->deallocs = 0;
  pool->head = alloc_arena(pool);
  if (pool->head == NULL) {
    free(pool);
    return 1;
  }
  pool->last = pool->head;
  pool->current = pool->head;
  *ptr = pool;
  return 0;
}

void* yrc_pool_attain(yrc_pool_t* pool) {
  yrc_pool_arena_t* cursor;
  int arena_pos;
retry:
  if (pool->current->free) {
    arena_pos = clz(pool->current->used_mask);
    pool->current->used_mask &= ~(1UL << (63 - arena_pos));
    --pool->current->free;
    return (void*)((size_t)pool->current->data + (size_t)(arena_pos * (pool->objsize + sizeof(yrc_pool_arena_t**))));
  }
  /* if we've seen no deallocations since last time, just skip forward. */
  if (!pool->deallocs) {
    pool->last->next = alloc_arena(pool);
    pool->last = pool->current = pool->last->next;
    goto retry;
  }
  cursor = pool->head;
  while (cursor) {
    if (cursor->free) {
      break;
    }
    cursor = cursor->next;
  }
  if (!cursor) {
    pool->deallocs = 0;
    goto retry;
  }
  pool->current = cursor;
  goto retry;
}

int yrc_pool_release(yrc_pool_t* pool, void* ptr) {
  void* baseptr = (void*)((size_t)ptr - sizeof(yrc_pool_arena_t**));
  yrc_pool_arena_t* arena = *(yrc_pool_arena_t**)(baseptr);
  arena->used_mask |= 1UL << ((uint_fast32_t)((size_t)baseptr - (size_t)arena->data) /
    (sizeof(yrc_pool_arena_t**) + pool->objsize));
  ++arena->free;
  pool->deallocs = 1;
  if (arena->free > pool->current->free) {
    pool->current = arena;
  }
  return 0;
}

int yrc_pool_free(yrc_pool_t* pool) {
  yrc_pool_arena_t* cursor = pool->head, *next;
  while (cursor) {
    next = cursor->next;
    free(cursor);
    cursor = next;
  }
  free(pool);
  return 0;
}
