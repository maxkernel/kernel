

#include <aul/queue.h>


queue_t queue_new(void * elems, size_t numelems, size_t elemsize)
{
	queue_t q;
	queue_init(&q, elems, numelems, elemsize);
	return q;
}

void queue_init(queue_t * queue, void * elems, size_t numelems, size_t elemsize)
{
	PZERO(queue, sizeof(queue_t));
	PZERO(elems, numelems*elemsize);
	queue->tail = elems;
	queue->head = NULL;
	queue->numelems = numelems;
	queue->elemsize = elemsize;
	queue->elems = elems;
}

bool queue_enqueue(queue_t * queue, void * elem)
{
	if (queue_isfull(queue))
	{
		return false;
	}

	memcpy(queue->tail, elem, queue->elemsize);

	// If the queue is empty (head pointer is null), set the head pointer
	if (queue->head == NULL)
	{
		queue->head = queue->tail;
	}

	// Increment the queue tail pointer
	queue->tail += queue->elemsize;
	if (queue->tail >= (queue->elems + (queue->elemsize * queue->numelems)))
	{
		// Wrap around if we've reached the end
		queue->tail = queue->elems;
	}

	return true;
}

void * queue_dequeue(queue_t * queue)
{
	if (queue_isempty(queue))
	{
		return NULL;
	}

	void * value = queue->head;

	// We just removed the an element from the head, increment the head pointer
	queue->head += queue->elemsize;
	if (queue->head >= (queue->elems + (queue->elemsize * queue->numelems)))
	{
		// Wrap around if we've reached the end
		queue->head = queue->elems;
	}

	if (queue->head == queue->tail)
	{
		// Queue is empty, make it so...
		queue->head = NULL;
	}

	return value;
}

void * queue_peak(queue_t * queue)
{
	return queue->head;
}

size_t queue_size(queue_t * queue)
{
	if (queue_isempty(queue))
	{
		return 0;
	}

	size_t i=0;
	void * ptr = queue->head;
	for (; ptr != queue->tail; ptr += queue->elemsize, i++)
	{
		if (ptr >= (queue->elems + (queue->elemsize * queue->numelems)))
		{
			// Wrap around if we've reached the end
			ptr = queue->elems;
		}
	}
}
