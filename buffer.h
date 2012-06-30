#ifndef __BUFFER_H
#define __BUFFER_H

#include <aul/common.h>
#include <aul/exception.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BUFFER_PAGESIZE		(4096)		// Must be less than 65536 (or 0x10000)

typedef struct __buffer_t buffer_t;

typedef struct
{
	// TODO - make this more intelligent (keep track of sub-buffers?)
	buffer_t * buffer;
	off_t offset;
} bufferpos_t;


bool buffer_init(size_t poolsize, exception_t ** err);
void buffer_destroy();

buffer_t * buffer_new();
buffer_t * buffer_dup(const buffer_t * src);

size_t buffer_write(buffer_t * buffer, const void * data, off_t offset, size_t length);
size_t buffer_read(const buffer_t * buffer, void * data, off_t offset, size_t length);
ssize_t buffer_send(const buffer_t * buffer, int fd, off_t offset, size_t length);

size_t buffer_size(const buffer_t * buffer);
void buffer_free(buffer_t * buffer);

void bufferpos_new(bufferpos_t * pos, buffer_t * buffer, size_t offset);
bool bufferpos_write(bufferpos_t * pos, const void * data, size_t length);
size_t bufferpos_read(bufferpos_t * pos, void * data, size_t length);
ssize_t bufferpos_send(bufferpos_t * pos, int fd, size_t length);
size_t bufferpos_remaining(const bufferpos_t * pos);
off_t bufferpos_seek(bufferpos_t * pos, off_t offset, int whence);
#define bufferpos_clear(b)	({ (b)->buffer = NULL; (b)->offset = 0; })

#ifdef __cplusplus
}
#endif
#endif
