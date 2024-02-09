#ifndef MS_COMMON_H
#define MS_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef MS_DEBUG
#define MS_DEBUG_MEM_ALLOC
#define MS_DEBUG_EXECUTION
#define MS_DEBUG_ASSERTIONS
#endif

#define MS_UNUSED(x) ((void)(x))

#ifdef MS_DEBUG_ASSERTIONS

#include <stdlib.h>
#include <stdio.h>

#define MS__ASSERT_STR "miniscript: invariant '%s' not upheld\n\tat file '%s', line %i\n"

#define MS_ASSERT_REASON(cond, reason) \
  if (!(cond))                         \
  {                                    \
    fprintf(stderr, MS__ASSERT_STR     \
      "reason: " reason,               \
      #cond, __FILE__, __LINE__        \
    );                                 \
    exit(-1);                          \
  }

#define MS_ASSERT(cond)             \
  if (!(cond))                      \
  {                                 \
    fprintf(stderr, MS__ASSERT_STR, \
      #cond, __FILE__, __LINE__     \
    );                              \
    exit(-1);                       \
  }

#else

#define MS_ASSERT_REASON(cond, reason)
#define MS_ASSERT(cond)

#endif // MS_DEBUG_ASSERTIONS

#endif // MS_COMMON_H
