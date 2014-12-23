#ifndef _YRC_POOL_H
#define _YRC_POOL_H

typedef struct yrc_pool_s yrc_pool_t;

int yrc_pool_init(yrc_pool_t**, size_t);
int yrc_pool_free(yrc_pool_t*);

void* yrc_pool_attain(yrc_pool_t*);
int yrc_pool_release(yrc_pool_t*, void*);

#endif
