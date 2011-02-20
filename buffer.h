#include <buffer2.h>

#ifndef __BUFFER_H
#define __BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#if 0
typedef unsigned char * buffer_t;

buffer_t buffer_new(size_t size);
buffer_t buffer_copy(const buffer_t src);

void buffer_append(buffer_t * buffer, void * data, size_t size);
void buffer_setdatasize(buffer_t * buffer, size_t size);

size_t buffer_size(const buffer_t buffer);
void * buffer_data(buffer_t buffer);
size_t buffer_datasize(const buffer_t buffer);

void buffer_free(buffer_t buffer);
#endif

#ifdef __cplusplus
}
#endif
#endif
