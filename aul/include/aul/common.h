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

#define MILLIS_PER_SECOND		1000
#define MICROS_PER_SECOND		1000000L
#define NANOS_PER_SECOND		1000000000LL

#define SENTINEL		(1)

#define likely(x)       (__builtin_expect(!!(x), 1))
#define unlikely(x)     (__builtin_expect(!!(x), 0))

#define max(a, b)  (((a) > (b)) ? (a) : (b))
#define min(a, b)  (((a) < (b)) ? (a) : (b))
#define abs(a)	   (((a) < 0) ? -(a) : (a))
#define clamp(x, low, high)	(((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#define check_printf(fmt_arg, arg1)		__attribute__((format (printf, fmt_arg, arg1)))
#define unused(v)						((void)v)
#define labels(...)						__label__ __VA_ARGS__
#define threadlocal						__thread
#define nelems(a)						(sizeof(a) / sizeof(a[0]))


#ifdef __cplusplus
}
#endif
#endif
