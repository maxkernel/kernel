#ifndef __AUL_QUEUE_H
#define __AUL_QUEUE_H

#include <aul/common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	void * tail, * head;
	int numelems, elemsize;
	void * elems;
} queue_t;


queue_t queue_new(void * elems, size_t numelems, size_t elemsize);
void queue_init(queue_t * queue, void * elems, size_t numelems, size_t elemsize);
bool queue_enqueue(queue_t * queue, void * elem);
void * queue_dequeue(queue_t * queue);
void * queue_peak(queue_t * queue);
size_t queue_size(queue_t * queue);

inline bool queue_isempty(queue_t * queue)
{
	return queue->head == NULL;
}

inline bool queue_isfull(queue_t * queue)
{
	return queue->tail == queue->head;
}


#define QUEUE(t)			queue_new(t, sizeof(t)/sizeof(t[0]), sizeof(t[0]))
#define QUEUE_INIT(q, t)	queue_init(&q, t, sizeof(t)/sizeof(t[0]), sizeof(t[0]))
#define ENQUEUE(q, e)		queue_enqueue(q, &e)
#define DEQUEUE(q, t)		(*(t*)queue_dequeue(q))


#ifdef __cplusplus
}
#endif
#endif
