#ifndef __ARRAY_H
#define __ARRAY_H

#include <stdbool.h>
#include <sys/types.h>

#include <buffer.h>
#include <kernel-types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef buffer_t array_t;

static inline size_t array_typesize(const char sig)
{
	switch (sig)
	{
		case T_ARRAY_BOOLEAN:	return sizeof(bool);
		case T_ARRAY_INTEGER:	return sizeof(int);
		case T_ARRAY_DOUBLE:	return sizeof(double);
		default:				return 0;
	}
}

static inline array_t array_new(const char type, size_t elems)
{
	array_t a = buffer_new();
	buffer_write(a, NULL, array_typesize(type) * elems, 0);
	return a;
}

static inline array_t array_dup(const array_t array)
{
	return buffer_dup(array);
}

static inline size_t array_size(const array_t array, const char type)
{
	return buffer_size(array) / array_typesize(type);
}

static inline void array_free(array_t array)
{
	buffer_free(array);
}

static inline size_t array_read(array_t array, const char type, off_t index, void * elems, size_t nelems)
{
	return buffer_read(array, elems, index * array_typesize(type), nelems * array_typesize(type));
}

static inline void array_write(array_t array, const char type, off_t index, void * elems, size_t nelems)
{
	buffer_write(array, elems, index * array_typesize(type), nelems * array_typesize(type));
}

#ifdef __cplusplus
}
#endif
#endif
