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

#define SENTINEL			(1)

// TODO - figure out performance implications on having the double !!'s (remove if necessary)
#define likely(x)       	(__builtin_expect(!!(x), 1))
#define unlikely(x)     	(__builtin_expect(!!(x), 0))

#define __concat(a,b)		a ## b
#define concat(a,b)			__concat(a,b)

#define callmacro(M,...)	M(__VA_ARGS__)

#define nargs(...)			__nargs(__VA_ARGS__,__nargs_seq_n())
#define __nargs(...)        __nargs_n(__VA_ARGS__)
#define __nargs_n( \
	_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,_21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32, \
	_33,_34,_35,_36,_37,_38,_39,_40,_41,_42,_43,_44,_45,_46,_47,_48,_49,_50,_51,_52,_53,_54,_55,_56,_57,_58,_59,_60,_61,_62,_63,N,...) N
#define __nargs_seq_n() \
	63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32, \
	31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0

#define max2(a,b)			({ typeof(a) __a = (a); typeof(b) __b = (b); (__a > __b)? __a : __b; })
#define max3(a,b,c)			({ max2(max2(a,b), (c)); })
#define max4(a,b,c,d)		({ max2(max2(a,b), max2(c,d)); })
#define max5(a,b,c,d,e)		({ max2(max4(a,b,c,d), (e)); })
#define max(...)			callmacro(concat(max, nargs(__VA_ARGS__)), __VA_ARGS__)

#define min2(a,b)			({ typeof(a) __a = (a); typeof(b) __b = (b); (__a < __b)? __a : __b; })
#define min3(a,b,c)			({ min2(min2(a,b), (c)); })
#define min4(a,b,c,d)		({ min2(min2(a,b), min2(c,d)); })
#define min5(a,b,c,d,e)		({ min2(min4(a,b,c,d), (e)); })
#define min(...)			callmacro(concat(min, nargs(__VA_ARGS__)), __VA_ARGS__)

#define abs(a)				({ typeof(a) __a = (a); (__a < 0)? -__a : __a })
#define clamp(x, low, high)	({ typeof(x) __x = (x); typeof(low) __low = (low); typeof(high) __high = (high); (__x > __high)? __high : ((__x < __low)? __low : __x); })

#define check_printf(fmt_arg, arg1)		__attribute__((format(printf, (fmt_arg), (arg1))))
#define unused(v)						((void)(v))
#define labels(...)						__label__ __VA_ARGS__
#define threadlocal						__thread
#define nelems(a)						(sizeof(a) / sizeof((a)[0]))



#ifdef __cplusplus
}
#endif
#endif
