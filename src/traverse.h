#ifndef _YRC_TRAVERSE_H
#define _YRC_TRAVERSE_H
#include "yrc-common.h"

typedef enum {
  kYrcTraverseContinue,
  kYrcTraverseStop
} yrc_visitor_mode;

typedef yrc_visitor_mode (*yrc_nodecb)(yrc_ast_node_t*, yrc_rel, yrc_ast_node_t*, void*);

typedef struct yrc_visitor_s {
  yrc_nodecb enter;
  yrc_nodecb exit;
} yrc_visitor_t;

void yrc_traverse(yrc_ast_node_t*, yrc_visitor_t*);

#endif
