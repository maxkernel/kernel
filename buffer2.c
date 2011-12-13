#include <sys/sendfile.h>
#include <unistd.h>
#include <errno.h>

#include <kernel.h>
#include <kernel-priv.h>

#include <buffer.h>

buffer_t buffer_new()
{
	return memfs_orphanfd();
}

buffer_t buffer_dup(buffer_t src)
{
	return memfs_dupfd(src);
}

void buffer_write(buffer_t buffer, const void * data, size_t size)
{
	ssize_t wrote = write(buffer, data, size);
	if (wrote != size)
	{
		LOGK(LOG_WARN, "Could not write data to buffer (size=%zu): %s", size, strerror(errno));
	}
}

size_t buffer_read(buffer_t buffer, void * data, size_t bufsize)
{
	ssize_t nread = read(buffer, data, bufsize);
	if (nread < 0)
	{
		LOGK(LOG_WARN, "Could not read data from buffer: %s", strerror(errno));
		return 0;
	}

	return nread;
}

bool buffer_send(buffer_t buffer, int sock)
{
	off_t off = 0;
	size_t size = buffer_size(buffer);

	ssize_t wrote = sendfile(sock, buffer, &off, size);
	if (wrote != size)
	{
		LOGK(LOG_WARN, "Could not send all data from buffer to socket (size=%zu): %s", size, strerror(errno));
		return false;
	}

	return true;
}

void buffer_setpos(buffer_t buffer, size_t pos)
{
	memfs_setseekfd(buffer, pos);
}

size_t buffer_getpos(buffer_t buffer)
{
	return memfs_getseekfd(buffer);
}

size_t buffer_size(buffer_t buffer)
{
	return memfs_sizefd(buffer);
}

void buffer_free(buffer_t buffer)
{
	//memfs_syncfd(buffer);	// BAAAD latency!
	memfs_closefd(buffer);
}
