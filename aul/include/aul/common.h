#ifndef __AUL_COMMON_H
#define __AUL_COMMON_H

#include <stdlib.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef  MILLIS_PER_SECOND
#define MILLIS_PER_SECOND		1000

#undef  MICROS_PER_SECOND
#define MICROS_PER_SECOND		1000000L

#undef  NANOS_PER_SECOND
#define NANOS_PER_SECOND		1000000000LL

#undef  LIKELY
#define LIKELY(x)       __builtin_expect(!!(x), 1)

#undef  UNLIKELY
#define UNLIKELY(x)     __builtin_expect(!!(x), 0)

#undef	MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef	MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#undef	ABS
#define ABS(a)	   (((a) < 0) ? -(a) : (a))

#undef	CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#undef  CHECK_PRINTF
#define CHECK_PRINTF(fmt_arg, arg1)		__attribute__((format (printf, fmt_arg, arg1)))


#define ZERO(x)			memset(&(x), 0, sizeof(x))
#define PZERO(x, size)	memset(x, 0, size)
#define FREE(x)			do { if (UNLIKELY((x) == NULL)) { break; } free(x); x = NULL; } while(0)
#define FREES(x)		do { if (UNLIKELY((x) == NULL)) { break; } free((char *)x); x = NULL; } while(0)

static inline void * malloc0(size_t size)
{
	void * ptr = malloc(size);
	PZERO(ptr, size);
	return ptr;
}

static inline char * STRDUP(const char * str)
{
	if (UNLIKELY(str == NULL))
	{
		return NULL;
	}
	else
	{
		return strdup(str);
	}
}

#ifdef __cplusplus
}
#endif
#endif
