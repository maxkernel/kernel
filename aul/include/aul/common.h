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

//#undef  MILLIS_PER_SECOND
#define MILLIS_PER_SECOND		1000

//#undef  MICROS_PER_SECOND
#define MICROS_PER_SECOND		1000000L

//#undef  NANOS_PER_SECOND
#define NANOS_PER_SECOND		1000000000LL

#define SENTINEL		(1)

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define max(a, b)  (((a) > (b)) ? (a) : (b))
#define min(a, b)  (((a) < (b)) ? (a) : (b))
#define abs(a)	   (((a) < 0) ? -(a) : (a))
// TODO - add abs_int, see mir bit twidling hacks!
#define clamp(x, low, high)  			(((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#undef  CHECK_PRINTF
#define CHECK_PRINTF(fmt_arg, arg1)		__attribute__((format (printf, fmt_arg, arg1)))


#define labels(...)						__label__ __VA_ARGS__
#define threadlocal						__thread

#undef  nelems
#define nelems(a)						(sizeof(a) / sizeof(a[0]))


//#define ZERO(x)			memset(&(x), 0, sizeof(x))
//#define PZERO(x, size)	memset(x, 0, size)
//#define FREE(x)			do { if (unlikely((x) == NULL)) { break; } free(x); x = NULL; } while(0)
#define FREES(x)		do { if (unlikely((x) == NULL)) { break; } free((char *)x); x = NULL; } while(0)

// TODO - remove this method!
static inline void * malloc0(size_t size)
{
	void * ptr = malloc(size);
	memset(ptr, 0, size);
	return ptr;
}

static inline char * STRDUP(const char * str)
{
	if (unlikely(str == NULL))
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
