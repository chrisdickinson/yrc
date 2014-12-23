#include "yrc-common.h"
#include "pool.h"
#include <stdlib.h> /* malloc + free */

#ifndef __builtin_clzll
  int clz(unsigned long long xs) {
    int i = sizeof(xs) << 3;
    int o = 0;
    while(i) {
      if (xs & (1 << i)) break;
      --i;
      ++o;
    }
    return o;
  }
#else 
  #define clz(xs) __builtin_clzll((unsigned long long)xs)
#endif

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
  uint_fast8_t free;
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
  for(uint_fast8_t i = 0; i < 64; ++i) {
    *iter = arena;
    iter += (sizeof(yrc_pool_arena_t**) + pool->objsize);
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
retry:
  if (pool->current->free) {
    pool->deallocs = 0;
    int arena_pos = clz(pool->current->used_mask);
    pool->current->used_mask &= ~1 << (63 - arena_pos);
    --pool->current->free;
    return (void*)pool->current->data + (size_t)(arena_pos * (pool->objsize + sizeof(yrc_pool_arena_t**)));
  }
  /* if we've seen no deallocations since last time, just skip forward. */
  if (!pool->deallocs) {
    pool->last->next = alloc_arena(pool);
    pool->last = pool->current = pool->last->next;
    goto retry;
  }
  yrc_pool_arena_t* cursor = pool->head;
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
  void* baseptr = ptr - sizeof(yrc_pool_arena_t**);
  yrc_pool_arena_t* arena = *(yrc_pool_arena_t**)(baseptr);
  arena->used_mask |= 1UL << ((uint_fast32_t)(baseptr - (void*)arena->data) /
    (sizeof(yrc_pool_arena_t**) + pool->objsize));
  ++arena->free;
  pool->deallocs = 1;
  if (arena->free > pool->current->free) {
    pool->current = arena;
  }
  return 0;
}
