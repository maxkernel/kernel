#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/uio.h>

#include <aul/mutex.h>
#include <aul/atomic.h>
#include <aul/stack.h>

#include <buffer.h>


#define TYPE_BUFFER 		0xB1
#define TYPE_PAGE			0xB2

#define PAGES_PER_BUFFER	((BUFFER_PAGESIZE - sizeof(buffer_t)) / sizeof(buffer_t *))
#define BYTES_PER_PAGE		(BUFFER_PAGESIZE - sizeof(page_t))
#define BYTES_PER_BUFFER	(BYTES_PER_PAGE * PAGES_PER_BUFFER)
#define PAGE_SIZE_OFFSET	(BYTES_PER_PAGE - sizeof(uint16_t))

#define page_size(p)		((uint16_t *)&(p)->data[PAGE_SIZE_OFFSET])


typedef struct
{
	uint8_t type;
	uint16_t refs;
	uint8_t data[0];
} page_t;

struct __buffer_t
{
	uint8_t type;
	size_t size;
	buffer_t * next;
	page_t * pages[0];
};


static void * memory = NULL;
static stack_t freepages;
static mutex_t freelock;

static const page_t * zero = NULL;

static inline void initpage(page_t * page)
{
	page->type = TYPE_PAGE;
	page->refs = 1;
	memset(page->data, 0, BYTES_PER_PAGE);
}

static inline void branchpage(const page_t * page, page_t * new)
{
	new->type = TYPE_PAGE;
	new->refs = 1;
	memcpy(new->data, page->data, BYTES_PER_PAGE);
}

static inline void initbuffer(buffer_t * buffer)
{
	buffer->type = TYPE_BUFFER;
	buffer->size = 0;
	buffer->next = NULL;
	memset(buffer->pages, 0, sizeof(page_t *) * PAGES_PER_BUFFER);
}

static inline void * getfree()
{
	void * page = NULL;
	mutex_lock(&freelock);
	{
		page = stack_pop(&freepages);
	}
	mutex_unlock(&freelock);

	return page;
}

static inline void putfree(void * page)
{
	mutex_lock(&freelock);
	{
		stack_push(&freepages, page);
	}
	mutex_unlock(&freelock);
}

static inline buffer_t * promote(page_t * page)
{
	page_t * new = NULL;
	uint16_t size = *page_size(page);

	if (size > 0)
	{
		new = getfree();
		if unlikely(new == NULL)
		{
			return NULL;
		}

		branchpage(page, new);
		*page_size(new) = 0;		// Clear out the size info of the new page
	}

	buffer_t * buffer = (buffer_t *)page;
	initbuffer(buffer);
	buffer->size = size;
	buffer->pages[0] = new;

	return buffer;
}

bool buffer_init(size_t poolsize, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}
	}

	stack_init(&freepages);
	mutex_init(&freelock, M_RECURSIVE);

	size_t pages = poolsize / BUFFER_PAGESIZE;
	int e = posix_memalign(&memory, BUFFER_PAGESIZE, BUFFER_PAGESIZE * pages);		// Allocate aligned memory
	if (e != 0 || memory == NULL)
	{
		exception_set(err, ENOMEM, "Could not allocte buffer space, currently set to %zu bytes: %s", poolsize, strerror(errno));
		memory = NULL;
		return false;
	}

	// Add pages to free list
	for (size_t i = 0; i < pages; i++)
	{
		list_t * list = (list_t *)(memory + (BUFFER_PAGESIZE * i));
		stack_push(&freepages, list);
	}

	// Create zero page
	{
		page_t * zeropage = getfree();
		if unlikely(zeropage == NULL)
		{
			exception_set(err, EFAULT, "Could not allocate zero page!");
			return false;
		}

		initpage(zeropage);
		zero = zeropage;
	}

	return true;
}

void buffer_destroy()
{
	stack_empty(&freepages);
	if (memory != NULL)
	{
		free(memory);
	}

	mutex_destroy(&freelock);
}

buffer_t * buffer_new()
{
	page_t * page = getfree();
	if unlikely(page == NULL)
	{
		return NULL;
	}

	initpage(page);
	return (buffer_t *)page;
}

buffer_t * buffer_dup(const buffer_t * src)
{
	// Sanity check
	{
		if unlikely(src == NULL)
		{
			return NULL;
		}
	}

	if (src->type == TYPE_PAGE)
	{
		// Src really points to a page, branch the page

		page_t * page = getfree();
		if unlikely(page == NULL)
		{
			return NULL;
		}

		branchpage((const page_t *)src, page);
		return (buffer_t *)page;
	}
	else if (src->type == TYPE_BUFFER)
	{
		// Src points to a buffer, do a shallow copy

		buffer_t * buffer = getfree();
		if unlikely(buffer == NULL)
		{
			return NULL;
		}

		initbuffer(buffer);
		buffer->size = src->size;

		for (size_t index = 0; index < PAGES_PER_BUFFER; index++)
		{
			page_t * page = src->pages[index];
			if (page != NULL)
			{
				atomic_inc(page->refs);
				buffer->pages[index] = page;
			}
		}

		if (src->next != NULL)
		{
			if ((buffer->next = buffer_dup(src->next)) == NULL)
			{
				// Out of buffer memory!
				buffer_free(buffer);
				return NULL;
			}
		}
		return buffer;
	}

	return NULL;
}

size_t buffer_write(buffer_t * buffer, const void * data, off_t offset, size_t length)
{
	// Sanity check
	{
		if unlikely(buffer == NULL || data == NULL)
		{
			return 0;
		}
	}

	if (buffer->type == TYPE_PAGE)
	{
		// We are writing to a single page
		size_t ensuresize = offset + length;
		page_t * page = (page_t *)buffer;

		if (ensuresize > (BYTES_PER_PAGE - sizeof(uint16_t)))
		{
			// We need more room than this page, promote this page to a full buffer
			// Let the next root if-statement catch it and do the writing
			if ((buffer = promote(page)) == NULL)
			{
				// Could not promote buffer (probably due to not enough free buffers)
				return 0;
			}
		}
		else
		{
			// We have enough room in this page to write the data
			uint16_t * size = page_size(page);
			if (offset > *size)
			{
				// Make sure that the memory between page->size and offset are filled with zeros
				memset(&page->data[*size], 0, offset - *size);
			}

			memcpy(&page->data[offset], data, length);
			*size = ensuresize;
		}
	}

	if (buffer->type == TYPE_BUFFER)
	{
		// Pass onto the next
		if (offset >= BYTES_PER_BUFFER)
		{
			// Make sure that buffer has a next
			if (buffer->next == NULL)
			{
				buffer->next = buffer_new();
			}

			size_t totalwrote = buffer_write(buffer->next, data, offset - BYTES_PER_BUFFER, length);
			if (totalwrote > 0)
			{
				// Only update the buffer size after successful write
				buffer->size = BYTES_PER_BUFFER;
			}
			else
			{
				// If error, put the buffer back into the free queue
				putfree(buffer->next);
				buffer->next = NULL;
			}

			return totalwrote;
		}

		size_t totalwrote = 0;
		size_t pagenum = offset / BYTES_PER_PAGE;
		off_t pageoff = offset % BYTES_PER_PAGE;
		size_t given = offset;

		while (length > 0)
		{
			// Check for overflow
			if (pagenum == PAGES_PER_BUFFER)
			{
				// Overflowed this buffer, start writing to the next one
				if (buffer->next == NULL)
				{
					buffer->next = buffer_new();
				}

				return totalwrote + buffer_write(buffer->next, data, 0, length);
			}

			page_t * page = buffer->pages[pagenum];

			// Check for null page
			if (page == NULL)
			{
				page_t * new = getfree();
				if unlikely(new == NULL)
				{
					// Could not allocate free page!
					return totalwrote;
				}

				initpage(new);
				page = buffer->pages[pagenum] = new;
			}

			// Check for readonly
			if (page->refs > 1)
			{
				page_t * new = getfree();
				if unlikely(new == NULL)
				{
					// Could not allocate free page
					return totalwrote;
				}

				branchpage(page, new);
				if (atomic_dec(page->refs) == 0)
				{
					putfree(page);
				}

				page = buffer->pages[pagenum] = new;
			}

			// Determine how many bytes to write
			size_t writelen = min(length, BYTES_PER_PAGE - pageoff);

			// Memcpy the bytes
			memcpy(&page->data[pageoff], data, writelen);

			// Update the buffer size parameter
			given += writelen;
			buffer->size = max(buffer->size, given);

			// Update the loop tracking variables
			length -= writelen;
			data += writelen;
			totalwrote += writelen;
			pagenum += 1;
			pageoff = 0;
		}

		return totalwrote;
	}

	return 0;
}

size_t buffer_read(const buffer_t * buffer, void * data, off_t offset, size_t length)
{
	// Sanity check
	{
		if unlikely(buffer == NULL || data == NULL)
		{
			return 0;
		}
	}

	if (buffer->type == TYPE_PAGE)
	{
		const page_t * page = (const page_t *)buffer;
		size_t size = *page_size(page);
		if (offset >= size)
		{
			return 0;
		}

		size_t bytes = min(length, size - offset);
		memcpy(data, &page->data[offset], bytes);
		return bytes;
	}
	else if (buffer->type == TYPE_BUFFER)
	{
		while (offset >= BYTES_PER_BUFFER)
		{
			buffer = buffer->next;
			offset -= BYTES_PER_BUFFER;

			if (buffer == NULL)
			{
				return 0;
			}
		}

		if (offset >= buffer->size)
		{
			return 0;
		}

		size_t totalread = 0;
		size_t pagenum = offset / BYTES_PER_PAGE;
		off_t pageoff = offset % BYTES_PER_PAGE;
		size_t left = buffer->size - offset;

		while (length > 0)
		{
			if (pagenum == PAGES_PER_BUFFER)
			{
				// Overflow this buffer, start reading from the next one
				buffer = buffer->next;
				if (buffer->next == NULL)
				{
					break;
				}

				left = buffer->size;
				pagenum = pageoff = 0;

				continue;
			}

			const page_t * page = buffer->pages[pagenum];

			// Check for null page
			if (page == NULL)
			{
				page = zero;
			}

			// Determine how many bytes to read
			size_t readlen = min(length, left, BYTES_PER_PAGE - pageoff);
			if (readlen == 0)
			{
				break;
			}

			// Read the bytes
			memcpy(data, &page->data[pageoff], readlen);

			// Update the loop tracking variables
			length -= readlen;
			left -= readlen;
			data += readlen;
			totalread += readlen;
			pagenum += 1;
			pageoff = 0;
		}

		return totalread;
	}

	return 0;
}

ssize_t buffer_send(const buffer_t * buffer, int fd, off_t offset, size_t length)
{
	// Sanity check
	{
		if unlikely(buffer == NULL)
		{
			errno = EINVAL;
			return -1;
		}
	}

	if (buffer->type == TYPE_PAGE)
	{
		const page_t * page = (const page_t *)buffer;
		size_t size = *page_size(page);
		if (offset >= size)
		{
			return 0;
		}

		size_t bytes = min(length, size - offset);
		return write(fd, &page->data[offset], bytes);
	}
	else if (buffer->type == TYPE_BUFFER)
	{
		size_t numpages = length / BYTES_PER_PAGE + 2;

		// Find starting buffer
		while (offset >= BYTES_PER_BUFFER)
		{
			buffer = buffer->next;
			offset -= BYTES_PER_BUFFER;

			if (buffer == NULL)
			{
				return 0;
			}
		}

		size_t index = 0;
		struct iovec vector[numpages];
		memset(vector, 0, sizeof(struct iovec) * numpages);


		size_t pagenum = offset / BYTES_PER_PAGE;
		off_t pageoff = offset % BYTES_PER_PAGE;
		size_t left = buffer->size - offset;

		while (length > 0)
		{
			if (pagenum == PAGES_PER_BUFFER)
			{
				// Overflow this buffer, start reading from the next one
				buffer = buffer->next;
				if (buffer->next == NULL)
				{
					break;
				}

				left = buffer->size;
				pagenum = pageoff = 0;

				continue;
			}

			const page_t * page = buffer->pages[pagenum];

			// Check for null page
			if (page == NULL)
			{
				page = zero;
			}

			// Determine how many bytes to read
			size_t readlen = min(length, left, BYTES_PER_PAGE - pageoff);
			if (readlen == 0)
			{
				break;
			}

			// Read the bytes
			vector[index].iov_base = (void *)&page->data[pageoff];		// Remove the const. Really iovec? That's just poor spec design!
			vector[index].iov_len = readlen;
			index += 1;

			// Update the loop tracking variables
			length -= readlen;
			left -= readlen;
			pagenum += 1;
			pageoff = 0;
		}

		if (index == 0)
		{
			return 0;
		}

		return writev(fd, vector, index);
	}

	errno = EINVAL;
	return -1;
}

size_t buffer_size(const buffer_t * buffer)
{
	// Sanity check
	{
		if unlikely(buffer == NULL)
		{
			return 0;
		}
	}

	if (buffer->type == TYPE_PAGE)
	{
		const page_t * page = (const page_t *)buffer;
		return *page_size(page);
	}
	else if (buffer->type == TYPE_BUFFER)
	{
		const buffer_t * next = buffer->next;
		if (next != NULL)
		{
			return buffer->size + buffer_size(next);
		}

		return buffer->size;
	}

	return 0;
}

void buffer_free(buffer_t * buffer)
{
	// Sanity check
	{
		if unlikely(buffer == NULL)
		{
			return;
		}
	}

	if (buffer->type == TYPE_PAGE)
	{
		putfree(buffer);
	}
	else if (buffer->type == TYPE_BUFFER)
	{
		for (size_t index = 0; index < PAGES_PER_BUFFER; index++)
		{
			page_t * page = buffer->pages[index];
			if (page != NULL)
			{
				if (atomic_dec(page->refs) == 0)
				{
					putfree(page);
				}
			}
		}

		if (buffer->next != NULL)
		{
			buffer_free(buffer->next);
		}

		putfree(buffer);
	}
}

bufferpos_t bufferpos_new(buffer_t * buffer)
{
	bufferpos_t pos = {
		.buffer = buffer,
		.offset = 0,
	};

	return pos;
}

bool bufferpos_write(bufferpos_t * pos, const void * data, size_t length)
{
	// Sanity check
	{
		if unlikely(pos == NULL)
		{
			return false;
		}
	}

	size_t wrote = buffer_write(pos->buffer, data, pos->offset, length);
	pos->offset += wrote;

	return wrote == length;
}

size_t bufferpos_read(bufferpos_t * pos, void * data, size_t length)
{
	// Sanity check
	{
		if unlikely(pos == NULL)
		{
			return 0;
		}
	}

	size_t read = buffer_read(pos->buffer, data, pos->offset, length);
	pos->offset += read;

	return read;
}

ssize_t bufferpos_send(bufferpos_t * pos, int fd, size_t length)
{
	// Sanity check
	{
		if unlikely(pos == NULL)
		{
			return 0;
		}
	}

	ssize_t sent = buffer_send(pos->buffer, fd, pos->offset, length);
	pos += (sent < 0)? 0 : sent;

	return sent;
}

off_t bufferpos_seek(bufferpos_t * pos, off_t offset, int whence)
{
	// Sanity check
	{
		if unlikely(pos == NULL || pos->buffer == NULL)
		{
			return -1;
		}
	}

	switch (whence)
	{
		case SEEK_SET:	pos->offset = offset;								break;
		case SEEK_CUR:	pos->offset += offset;								break;
		case SEEK_END:	pos->offset = buffer_size(pos->buffer) + offset;	break;
		default:		return -1;
	}

	return pos->offset;
}
