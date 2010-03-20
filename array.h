#ifndef __ARRAY_H
#define __ARRAY_H

#include <buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef buffer_t array_t;

#define array_new(type, elems)		buffer_new(sizeof(type) * elems)
#define array_copy(array)		buffer_copy(array)
#define array_free(array)		buffer_free(array)
#define array_data(array, type)		((type *)(buffer_data(array)))
#define array_length(array, type)	(buffer_datasize(array) / sizeof(type))

#ifdef __cplusplus
}
#endif
#endif
