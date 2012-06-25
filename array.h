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

// TODO - maybe make this a macro or something??
static inline size_t array_typesize(char sig)
{
	switch (sig)
	{
		case T_ARRAY_BOOLEAN:	return sizeof(bool);
		case T_ARRAY_INTEGER:	return sizeof(int);
		case T_ARRAY_DOUBLE:	return sizeof(double);
		default:				return 0;
	}
}

static inline array_t * array_new()
{
	return buffer_new();
}

static inline array_t * array_dup(const array_t * array)
{
	return buffer_dup(array);
}

static inline size_t array_size(const array_t * array, char type)
{
	// Sanity check
	{
		if unlikely(array_typesize(type) == 0)
		{
			return 0;
		}
	}

	return buffer_size(array) / array_typesize(type);
}

static inline void array_free(array_t * array)
{
	buffer_free(array);
}

static inline size_t array_read(const array_t * array, char type, off_t index, void * elems, size_t nelems)
{
	// Sanity check
	{
		if unlikely(array_typesize(type) == 0 || index < 0)
		{
			return 0;
		}
	}

	return buffer_read(array, elems, index * array_typesize(type), nelems * array_typesize(type)) / array_typesize(type);
}

static inline size_t array_write(array_t * array, char type, off_t index, const void * elems, size_t nelems)
{
	// Sanity check
	{
		if unlikely(array_typesize(type) == 0 || index < 0)
		{
			return 0;
		}
	}

	return buffer_write(array, elems, index * array_typesize(type), nelems * array_typesize(type)) / array_typesize(type);
}

static inline bool array_readindex(const array_t * array, char type, off_t index, void * elem)
{
	// Sanity check
	{
		if unlikely(array_typesize(type) == 0 || index < 0)
		{
			return false;
		}
	}

	return array_read(array, type, index, elem, 1) == 1;
}

static inline bool array_writeindex(array_t * array, char type, off_t index, const void * elem)
{
	// Sanity check
	{
		if unlikely(array_typesize(type) == 0 || index < 0)
		{
			return false;
		}
	}

	return array_write(array, type, index, elem, 1) == 1;
}

#ifdef __cplusplus
}
#endif
#endif
