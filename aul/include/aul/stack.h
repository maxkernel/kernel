#ifndef __AUL_STACK_H
#define __AUL_STACK_H

#include <aul/common.h>
#include <aul/list.h>

typedef list_t stack_t;

#define stack_init(ptr)		({ list_init(ptr); })

static inline bool stack_isempty(stack_t * head)
{
	return list_isempty(head);
}

static inline void stack_push(stack_t * head, list_t * new)
{
	list_push(head, new);
}

static inline void stack_enqueue(stack_t * head, list_t * new)
{
	list_add(head, new);
}

static inline list_t * stack_pop(stack_t * head)
{
	if (stack_isempty(head))
	{
		return NULL;
	}

	list_t * top = list_next(head);
	list_remove(top);
	return top;
}


#endif
