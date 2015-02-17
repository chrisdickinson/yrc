#ifndef YRC_H
#define YRC_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#if defined(_MSC_VER) && _MSC_VER < 1600
# include "stdint-msvc2008.h"
#else
# include <stdint.h>
#endif

#ifdef _WIN32
  /* Windows - set up dll import/export decorators. */
# if defined(BUILDING_YRC_SHARED)
    /* Building shared library. */
#   define YRC_EXTERN __declspec(dllexport)
# elif defined(USING_YRC_SHARED)
    /* Using shared library. */
#   define YRC_EXTERN __declspec(dllimport)
# else
    /* Building static library. */
#   define YRC_EXTERN /* nothing */
# endif
#elif __GNUC__ >= 4
# define YRC_EXTERN __attribute__((visibility("default")))
#else
# define YRC_EXTERN /* nothing */
#endif

#include "yrc_llist.h"
#include "yrc_tokens.h"
#include "yrc_ast.h"
#include "yrc_str.h"

typedef size_t (*yrc_readcb)(char*, size_t, void*);
typedef struct yrc_parse_request_s {
  yrc_readcb      read;
  size_t          readsize;
  void*           readctx;
} yrc_parse_request_t;

typedef struct yrc_parse_response_s {
  yrc_ast_node_t* root;
  yrc_error_t*    error;
} yrc_parse_response_t;

YRC_EXTERN int yrc_parse(yrc_parse_request_t*, yrc_parse_response_t**);
YRC_EXTERN int yrc_parse_free(yrc_parse_response_t*);
YRC_EXTERN int yrc_error(yrc_error_t*, char*, size_t);
YRC_EXTERN int yrc_error_token(yrc_error_t*, const char**);
YRC_EXTERN int yrc_error_position(yrc_error_t*, size_t*, size_t*, size_t*);

#ifdef __cplusplus
}
#endif
#endif
