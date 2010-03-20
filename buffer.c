#include <string.h>
#include <glib.h>
#include "buffer.h"

#if defined(KERNEL)
	#include "kernel.h"
	#include "kernel-priv.h"
#endif

buffer_t buffer_new(size_t size)
{
	size += sizeof(size_t);

	buffer_t buffer = g_slice_alloc0(size);
	*(size_t *)buffer = size;

	return buffer;
}

buffer_t buffer_copy(const buffer_t src)
{
	if (src == NULL)
	{
		return NULL;
	}

	return g_slice_copy(buffer_size(src), src);
}

void buffer_append(buffer_t * buffer, void * data, size_t size)
{
	size_t oldsize = buffer_datasize(*buffer);
	buffer_setdatasize(buffer, oldsize + size);
	if (data != NULL)
	{
		memcpy(buffer_data(*buffer)+oldsize, data, size);
	}
}

void buffer_setdatasize(buffer_t * buffer, size_t size)
{
	size_t oldsize = buffer_size(*buffer);
	size_t newsize = size + sizeof(size_t);

	buffer_t new_buffer = g_slice_alloc0(newsize);
	memcpy(new_buffer, *buffer, MIN(oldsize, newsize));
	*(size_t *)new_buffer = newsize;

	buffer_free(*buffer);
	*buffer = new_buffer;
}

size_t buffer_size(const buffer_t buffer)
{
	return *(size_t *)buffer;
}

void * buffer_data(buffer_t buffer)
{
	return (char *)(buffer+sizeof(size_t));
}

size_t buffer_datasize(const buffer_t buffer)
{
	return buffer_size(buffer) - sizeof(size_t);
}

void buffer_free(buffer_t buffer)
{
	if (buffer != NULL)
	{
		g_slice_free1(buffer_size(buffer), buffer);
	}
}
