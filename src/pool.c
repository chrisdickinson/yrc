#include "yrc-common.h"
#include "pool.h"
#include <stdlib.h> /* malloc + free */

// TODO: make this portable:
#define clz(xs) __builtin_clzll(xs)
/**

arenaptr
  |
  V
  +------------+------------+------------+---------------------------------------------+
  | next       | used_mask  | free       | data                                        |
  |            |            |            +----------+-----------+----------+-----------+
  |            |            |            | arenaptr | obj....   | arenaptr | obj...    |
  +------------+------------+------------+----------+-----------+----------+-----------+
  used_mask "1" bits indicate "free space", "0" bits indicate "occupied"
  to allocate:
    1. find the first "1" bit (using __builtin_clzll so *hopefully* speedy) in pool->current,
      1. if found:
      2. set it to zero
      3. decrement free
      4. return pointer to obj
    2. if not found, and pool->dealloc is not set,
      1. create a new arena at the end of the pool, set pool->current, retry
    3. if not found and pool->dealloc *is* set, find the first
       arena with free space, retry.
       4. if we're all filled up and deallocs is true, then 
        A) we're living in a lie from which there is no waking
        B) set deallocs to false and retry because the dream is collapsing

  to deallocate:
    1. subtract 8 from the pointer address, that is a pointer to the arena pointer
    2. figure out the offset in the arena that the arenapointer represents,
    3. unset that bit
    4. increment free
    5. set dealloc = 1
    6. if arena->free > pool->current->free, set pool->current to this arena


  all in all:
  * only need to malloc once for every FREE_SIZE objects
  * allocations out of an arena with room are O(1)
  * allocations out of an arena without room whose pool hasn't deallocated are malloc + O(64)
     basically O(1)
  * allocations out of an arena without room whose pool has deallocated are O(N)
  * deallocations are O(1) (and may help avoid O(N) search if a bunch are grouped)
**/

#define MASK_SIZE 2
#define FREE_SIZE (MASK_SIZE * 64)

typedef struct yrc_pool_arena_s {
  struct yrc_pool_arena_s* next;
  size_t used_mask[MASK_SIZE];
  uint_fast32_t free;
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
  uint_fast32_t i;
  arena = malloc(sizeof(yrc_pool_arena_t) - 1 + (sizeof(yrc_pool_arena_t**) + pool->objsize) * FREE_SIZE);
  if (arena == NULL) {
    return NULL;
  }
  arena->next = NULL;
  for(i = 0; i < MASK_SIZE; ++i) {
    arena->used_mask[i] = 0xFFFFFFFFFFFFFFFF;
  }
  arena->free = FREE_SIZE;
  ++pool->num_arenas;
  iter = (yrc_pool_arena_t**)arena->data;
  for(i = 0; i < FREE_SIZE; ++i) {
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
  int i;
retry:
  if (pool->current->free) {
    for (i = 0; i < MASK_SIZE; ++i) {
      if (pool->current->used_mask[i]) {
        arena_pos = clz(pool->current->used_mask[i]);
        pool->current->used_mask[i] &= ~(1UL << (63 - arena_pos));
        --pool->current->free;
        return (void*)((size_t)pool->current->data + (size_t)((arena_pos + (i<<6)) * (pool->objsize + sizeof(yrc_pool_arena_t**))));
      }
    }
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
  int arena_pos = ((uint_fast32_t)((size_t)baseptr - (size_t)arena->data) /
    (sizeof(yrc_pool_arena_t**) + pool->objsize));
  arena->used_mask[arena_pos >> 6] |= 1UL << (arena_pos & 63);
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
