#ifndef __AUL_COMMON_H
#define __AUL_COMMON_H

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __BOOLEAN
#define __BOOLEAN
typedef int boolean;
#endif

#ifndef TRUE
# define TRUE		1
#endif

#ifndef FALSE
# define FALSE		0
#endif

#ifndef MICROS_PER_SECOND
# define MICROS_PER_SECOND		1000000LL
#endif

#ifndef NANOS_PER_SECOND
# define NANOS_PER_SECOND		1000000000LL
#endif

#define LIKELY(x)       __builtin_expect(!!(x), 1)
#define UNLIKELY(x)     __builtin_expect(!!(x), 0)

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
	if (UNLIKELY(str == NULL)){
		return NULL;
	}

	return strdup(str);
}

#ifdef __cplusplus
}
#endif
#endif
