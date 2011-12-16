#ifndef __BUFFER_H
#define __BUFFER_H

#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int buffer_t;

void buffer_init();

buffer_t buffer_new();
buffer_t buffer_dup(const buffer_t src);

void buffer_write(buffer_t buffer, const void * data, off_t offset, size_t length);
size_t buffer_read(buffer_t buffer, void * data, off_t offset, size_t length);
bool buffer_send(buffer_t buffer, int sock);

size_t buffer_size(const buffer_t buffer);
void buffer_free(buffer_t buffer);

#ifdef __cplusplus
}
#endif
#endif
