#ifndef MS_COMMON_H
#define MS_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef MS_DEBUG
#define MS_DEBUG_MEM_ALLOC
#define MS_DEBUG_EXECUTION
#define MS_DEBUG_PRINT_CODE
#define MS_DEBUG_ASSERTIONS
#define MS_DEBUG_COMPILATION
#endif

#define MS_UNUSED(x) ((void)(x))

#ifdef MS_DEBUG_ASSERTIONS

#include <stdlib.h>
#include <stdio.h>

#define MS__ASSERT_STR "miniscript: invariant '%s' not upheld\n\tat file '%s', line %i\n"

#define MS_ASSERT_REASON(cond, reason) do { \
    if (!(cond))                              \
    {                                         \
      fprintf(stderr, MS__ASSERT_STR          \
        "reason: " reason,                    \
        #cond, __FILE__, __LINE__             \
      );                                      \
      exit(-1);                               \
    }                                         \
  } while(0)

#define MS_ASSERT(cond) do {          \
    if (!(cond))                      \
    {                                 \
      fprintf(stderr, MS__ASSERT_STR, \
        #cond, __FILE__, __LINE__     \
      );                              \
      exit(-1);                       \
    }                                 \
  } while(0)

#define MS_UNREACHABLE(place) do {            \
    fprintf(stderr,                           \
      "miniscript: reached branch in '" place \
      "' marked as unreachable\n"             \
      "\tat file '%s', line %i\n",            \
      __FILE__, __LINE__                      \
    );                                        \
    exit(-1);                                 \
  } while(0)

#else

#define MS_ASSERT_REASON(cond, reason) exit(-1)
#define MS_ASSERT(cond) exit(-1)
#define MS_UNREACHABLE(place) exit(-1)

#endif // MS_DEBUG_ASSERTIONS

#define UINT8_COUNT (UINT8_MAX+1)

#endif // MS_COMMON_H
