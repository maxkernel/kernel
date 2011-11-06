#ifndef __BUFFER_H
#define __BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int buffer_t;

buffer_t buffer_new();
buffer_t buffer_dup(buffer_t src);

void buffer_write(buffer_t buffer, const void * data, size_t size);
size_t buffer_read(buffer_t buffer, void * data, size_t bufsize);
bool buffer_send(buffer_t buffer, int sock);

void buffer_setpos(buffer_t buffer, size_t pos);
size_t buffer_getpos(buffer_t buffer);

size_t buffer_size(buffer_t buffer);
void buffer_free(buffer_t buffer);

#ifdef __cplusplus
}
#endif
#endif
