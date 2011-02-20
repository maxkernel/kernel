#ifndef __AUL_QUEUE_H
#define __AUL_QUEUE_H

#include <aul/common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	void * tail, * head;
	size_t bufsize;
	void * buffer;
} queue_t;

#define QUEUE_INIT(q, b, s)	do { \
	PZERO((q), sizeof(queue_t));				\
	(q)->buffer = (b);  (q)->bufsize = (s);		\
	(q)->head = NULL; (q)->tail = (q)->buffer;	\
} while (0)


static inline void __queue_unwrap(queue_t * queue, void * head, void * target, size_t size)
{
	// First unwrap up to end of buffer
	void * end = MIN(head + size, queue->buffer + queue->bufsize);
	size_t bytes = end - head;
	memcpy(target, head, bytes);

	if (bytes < size)
	{
		// The buffer wrapped, so copy the rest
		memcpy(target + bytes, queue->buffer, size - bytes);
	}
}

static inline void __queue_wrap(queue_t * queue, void * tail, void * src, size_t size)
{
	void * end = MIN(tail + size, queue->buffer + queue->bufsize);
	size_t bytes = end - tail;
	memcpy(tail, src, bytes);

	if (bytes < size)
	{
		// The buffer wrapped, so copy the rest
		memcpy(queue->buffer, src + bytes, size - bytes);
	}
}

static inline void __queue_advance(queue_t * queue, void ** what, size_t length)
{
	*what += length;
	if (*what >= (queue->buffer + queue->bufsize))
	{
		// Overshot end of queue, wrap
		*what = queue->buffer + (*what - (queue->buffer + queue->bufsize));
	}
}

inline bool queue_isempty(queue_t * queue)
{
	return queue->head == NULL;
}

inline bool queue_isfull(queue_t * queue)
{
	return queue->tail == queue->head;
}

static inline size_t queue_size(queue_t * queue)
{
	if (queue_isempty(queue))
	{
		return 0;
	}
	else if (queue_isfull(queue))
	{
		return queue->bufsize;
	}

	if (queue->tail < queue->head)
	{
		// Queue has wrapped
		return queue->bufsize - (queue->head - queue->tail);
	}
	else
	{
		return queue->tail - queue->head;
	}
}

static inline queue_t queue_new(void * buffer, size_t bufsize)
{
	queue_t q;
	QUEUE_INIT(&q, buffer, bufsize);
	return q;
}

static inline bool queue_enqueue(queue_t * queue, void * data, size_t size)
{
	// Sanity check
	if (UNLIKELY(size == 0))
	{
		return true;
	}

	// 1) Determine if there is enough free space
	size_t queuesize = queue_size(queue);
	if ((queuesize + size) > queue->bufsize)
	{
		// Can't fit all bytes in queue
		return false;
	}

	// We may need to set the head pointer if it's null
	void * newhead = queue->head == NULL? queue->tail : NULL;

	// 2) Wrap the data
	__queue_wrap(queue, queue->tail, data, size);

	// 3) Advance the tail pointer
	__queue_advance(queue, &queue->tail, size);

	if (newhead != NULL)
	{
		// We must update the head pointer because it was NULL (indicating an empty queue)
		queue->head = newhead;
	}

	return true;
}

static inline bool queue_dequeue(queue_t * queue, void * target, size_t size)
{
	// Sanity check
	if (UNLIKELY(size == 0))
	{
		return true;
	}

	//  1) Determine if we have enough data in queue
	size_t queuesize = queue_size(queue);
	if (queuesize < size)
	{
		// Not enough bytes in queue
		return false;
	}

	// Save the head pointer
	void * head = queue->head;

	// 2) Advance the head pointer
	if (queuesize == size)
	{
		// Queue will be empty, so set head to NULL
		queue->head = NULL;
	}
	else
	{
		__queue_advance(queue, &queue->head, size);
	}

	// 3) Unwrap the data
	__queue_unwrap(queue, head, target, size);

	return true;
}


#ifdef __cplusplus
}
#endif
#endif
