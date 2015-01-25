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

#ifndef FPOS_T
#define FPOS_T uint64_t
#endif

typedef enum {
  YRC_OK,
  YRC_EUNEXPECTED,
  YRC_ENOTALLOWED,
  YRC_EBADTOKEN,
  YRC_EMEM
} yrc_parse_error_type;
typedef struct yrc_error_s yrc_error_t;

typedef size_t (*yrc_readcb)(char*, size_t);
typedef struct yrc_parser_s yrc_parser_t;

YRC_EXTERN int yrc_parse(yrc_readcb, yrc_error_t**);
YRC_EXTERN int yrc_error(yrc_error_t*, char*, size_t);
YRC_EXTERN int yrc_error_token(yrc_error_t*, const char**);
YRC_EXTERN int yrc_error_position(yrc_error_t*, FPOS_T*, FPOS_T*, FPOS_T*);

#ifdef __cplusplus
}
#endif
#endif
