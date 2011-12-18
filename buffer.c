#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <aul/common.h>
#include <aul/list.h>
#include <aul/mutex.h>
#include <kernel-priv.h>
#include <buffer.h>


#define BUFFER_POOL_SIZE		(20 * 1024 * 1024)		// 20 MB
#define BUFFER_MAX_PAGES		32						// Maximum number of pages we track per buffermem_t
#define BUFFER_MAX_BUFFERS		256						// Maximum number if buffers to track

typedef struct
{
	list_t empty_list;
	mutex_t access_lock;

	unsigned int references;
	
	void * pages[BUFFER_MAX_PAGES];
	size_t length;

	int next;
} buffermem_t;


static buffermem_t * buffers[BUFFER_MAX_BUFFERS];
static size_t PAGESIZE;
static size_t BUFFERSIZE;

static mutex_t buffers_lock;
static mutex_t pages_lock;

static list_t empty_buffers;
static list_t empty_pages;

static inline bool buffer_invalid(const buffer_t b)
{
	return b < 0 || b >= BUFFER_MAX_BUFFERS || buffers[b] == NULL;
}

static void * page_getnew()
{
	void * page = NULL;
	mutex_lock(&pages_lock);
	{
		if (!list_isempty(&empty_pages))
		{
			list_t * list = page = empty_pages.next;
			list_remove(list);
		}
	}
	mutex_unlock(&pages_lock);

	return page;
}

static int buffer_getnew()
{
	int index = -1;

	mutex_lock(&buffers_lock);
	{
		if (!list_isempty(&empty_buffers))
		{
			buffermem_t * buf = list_entry(empty_buffers.next, buffermem_t, empty_list);
			list_remove(&buf->empty_list);

			memset(buf->pages, 0, sizeof(buf->pages));
			buf->length = 0;
			buf->references = 1;
			buf->next = -1;

			int i=0;
			// We know that there is at lease one open buffers[], so don't need to check overflow
			while (buffers[i] != NULL)
			{
				i += 1;
			}

			buffers[i] = buf;
			index = i;
		}
	}
	mutex_unlock(&buffers_lock);

	return index;
}

static size_t buffer_extend(buffermem_t * m, size_t size)
{
	LABELS(done);

	size_t newsize = 0;
	mutex_lock(&m->access_lock);
	{
		for (size_t i=0; i<BUFFER_MAX_PAGES && size>0; i++)
		{
			if (m->pages[i] == NULL)
			{
				void * p = m->pages[i] = page_getnew();
				if (p == NULL)
				{
					goto done;
				}
			}

			size_t s = MIN(size, PAGESIZE);
			size -= s;
			m->length = newsize += s;
		}

		if (size > 0)
		{
			if (m->next == -1)
			{
				int newbuf = m->next = buffer_getnew();
				if (newbuf == -1)
				{
					goto done;
				}
			}

			newsize += buffer_extend(buffers[m->next], size);
		}
	}

done:
	mutex_unlock(&m->access_lock);

	return newsize;
}

void buffer_init()
{
	PAGESIZE = getpagesize();
	BUFFERSIZE = PAGESIZE * BUFFER_MAX_PAGES;
	LIST_INIT(&empty_buffers);
	LIST_INIT(&empty_pages);
	mutex_init(&buffers_lock, M_RECURSIVE);
	mutex_init(&pages_lock, M_RECURSIVE);
	
	// Zero out the buffers pointer array
	memset(buffers, 0, sizeof(buffers));
	
	// Allocate the buffers
	buffermem_t * buffers_mem = calloc(BUFFER_MAX_BUFFERS, sizeof(buffermem_t));	// TODO - align to page boundry

	// Add the buffers to the empty_buffers list
	for (size_t i=0; i<BUFFER_MAX_BUFFERS; i++)
	{
		buffermem_t * buf = &buffers_mem[i];
		mutex_init(&buf->access_lock, M_RECURSIVE);

		list_add(&empty_buffers, &buf->empty_list);
	}

	// Allocate the *huge* page pool
	size_t pages = BUFFER_POOL_SIZE / PAGESIZE;
	char * pages_mem = NULL;
	int e = posix_memalign((void **)&pages_mem, PAGESIZE, PAGESIZE * pages);		// Allocate aligned memory
	if (e != 0 || pages_mem == NULL)
	{
		LOGK(LOG_FATAL, "Could not allocte buffer space, currently set to %d bytes: %s", BUFFER_POOL_SIZE, strerror(errno));
	}

	// Add pages to free list
	for (size_t i=0; i<pages; i++)
	{
		list_t * list = (list_t *)&pages_mem[PAGESIZE * i];
		list_add(&empty_pages, list);
	}
}

void buffer_destroy()
{
	// TODO - destroy the buffers, warn is there are any open ones still
}

buffer_t buffer_new()
{
	return buffer_getnew();
}

buffer_t buffer_dup(const buffer_t b)
{
	__sync_add_and_fetch(&buffers[b]->references, 1);		// Atomic increment
	return (buffer_t)b;
}

static void buffer_dowrite(buffer_t b, char * data, off_t offset, size_t length)
{
	LABELS(done);

	mutex_lock(&buffers[b]->access_lock);
	{
		if (offset >= BUFFERSIZE)
		{
			buffer_dowrite(buffers[b]->next, data, offset - BUFFERSIZE, length);
			goto done;
		}

		size_t pagenum = offset / PAGESIZE;
		off_t pageoff = offset % PAGESIZE;

		while (length > 0)
		{
			if (pagenum == BUFFER_MAX_PAGES)
			{
				// Overwrote the buffer, start writing to the next one
				buffer_dowrite(buffers[b]->next, data, 0, length);
				goto done;
			}

			size_t writelen = MIN(length, PAGESIZE - pageoff);
			memcpy(buffers[b]->pages[pagenum] + pageoff, data, writelen);

			length -= writelen;
			data += writelen;
			pagenum += 1;
			pageoff = 0;
		}
	}

done:
	mutex_unlock(&buffers[b]->access_lock);
}

void buffer_write(buffer_t b, const void * data, off_t offset, size_t length)
{
	if (buffer_invalid(b))
	{
		return;
	}

	size_t size = buffer_size(b);
	size_t newsize = offset + length;

	if (newsize > size)
	{
		buffer_extend(buffers[b], newsize);
	}

	buffer_dowrite(b, (char *)data, offset, length);
}

static void buffer_doread(const buffer_t b, char * data, off_t offset, size_t length)
{
	LABELS(done);

	mutex_lock(&buffers[b]->access_lock);
	{
		if (offset >= BUFFERSIZE)
		{
			buffer_doread(buffers[b]->next, data, offset - BUFFERSIZE, length);
			goto done;
		}

		size_t pagenum = offset / PAGESIZE;
		off_t pageoff = offset % PAGESIZE;

		while (length > 0)
		{
			if (pagenum == BUFFER_MAX_PAGES)
			{
				buffer_doread(buffers[b]->next, data, 0, length);
				goto done;
			}

			size_t readlen = MIN(length, PAGESIZE - pageoff);
			memcpy(data, buffers[b]->pages[pagenum] + pageoff, readlen);

			length -= readlen;
			data += readlen;
			pagenum += 1;
			pageoff = 0;
		}
	}

done:
	mutex_unlock(&buffers[b]->access_lock);
}

size_t buffer_read(const buffer_t b, void * data, off_t offset, size_t length)
{
	if (buffer_invalid(b))
	{
		return 0;
	}

	size_t size = buffer_size(b);
	size_t readsize = offset + length;

	if (offset > size)
	{
		// Can't read any data! Offset is larger then entire buffer
		return 0;
	}

	if (readsize > size)
	{
		// Can't read that much, adjust the length
		length -= readsize - size;
	}

	buffer_doread(b, (char *)data, offset, length);
	return length;
}

bool buffer_send(buffer_t buffer, int sock)
{
	return false;
}

size_t buffer_size(const buffer_t b)
{
	if (buffer_invalid(b))
	{
		return 0;
	}

	size_t s = 0;
	mutex_lock(&buffers[b]->access_lock);
	{
		s = buffers[b]->length + buffer_size(buffers[b]->next);
	}
	mutex_unlock(&buffers[b]->access_lock);
	return s;
}

void buffer_free(buffer_t b)
{
	if (buffer_invalid(b))
	{
		return;
	}


	int refs = __sync_sub_and_fetch(&buffers[b]->references, 1);		// Atomic decrement
	if (refs > 0)
	{
		// Still references
		return;
	}

	mutex_lock(&buffers[b]->access_lock);
	{
		// No more references to this buffer, free it
		buffer_free(buffers[b]->next);

		// Loop through the pages and free each one
		mutex_lock(&pages_lock);
		{
			for (size_t i=0; i<BUFFER_MAX_PAGES; i++)
			{
				list_t * list = (list_t *)buffers[b]->pages[i];
				if (list == NULL)
				{
					break;
				}

				// Got a valid page, add it to the empty_pages list
				list_add(&empty_pages, list);
			}
		}
		mutex_unlock(&pages_lock);
	}
	mutex_unlock(&buffers[b]->access_lock);

	mutex_lock(&buffers_lock);
	{
		// Add the free'd buffer to the empty list
		list_add(&empty_buffers, &buffers[b]->empty_list);
		buffers[b] = NULL;
	}
	mutex_unlock(&buffers_lock);
}

