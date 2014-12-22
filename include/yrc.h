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

typedef size_t (*yrc_readcb)(char*, size_t);
YRC_EXTERN int yrc_parse(yrc_readcb);
#ifdef __cplusplus
}
#endif
#endif
