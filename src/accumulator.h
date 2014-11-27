#ifndef _YRC_ACCUM_H
#define _YRC_ACCUM_H

typedef struct yrc_accum_s yrc_accum_t;
int yrc_accum_init(yrc_accum_t**, size_t base_size);
int yrc_accum_push(yrc_accum_t*, char);
int yrc_accum_copy(yrc_accum_t*, char*, size_t);
int yrc_accum_free(yrc_accum_t*);
int yrc_accum_export(yrc_accum_t*, char**, size_t*);
int yrc_accum_borrow(yrc_accum_t*, char**, size_t*);
int yrc_accum_discard(yrc_accum_t*);

#endif
