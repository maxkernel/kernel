#ifndef __AUL_LIST_H
#define __AUL_LIST_H

#include <aul/common.h>


typedef struct __list_t
{
	struct __list_t *next, *prev;
} list_t;

typedef int (*list_compare_f)(list_t * a, list_t * b);


#define LIST_INIT(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)


/*
 * Insert a new entry between two known consecutive entries. 
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_append(list_t *new, list_t *prev, list_t *next)
{
	new->next = next;
	new->prev = prev;
	next->prev = new;
	prev->next = new;
}

/**
 * list_add - add a new entry to the end of the list
 * @head: list head to add it before
 * @new: new entry to be added
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add(list_t * head, list_t * new)
{
	__list_append(new, head->prev, head);
}

/**
 * list_push - add a new entry to the beginning of the list
 * @head: list head to add it before
 * @new: new entry to be added
 *
 * Insert a new entry at the specified head.
 * This is useful for implementing queues.
 */
static inline void list_push(list_t * head, list_t * new)
{
	__list_append(new, head, head->next);
}

#if 0
/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_del(list_t *prev, list_t *next)
{
	next->prev = prev;
	prev->next = next;
}
#endif

/**
 * list_remove - removes an entry from list.
 * @entry: the element to delete from the list.
 * Note: list_isempty on entry does not return true after this, the entry is in an undefined state.
 */
static inline void list_remove(list_t * entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	entry->next = NULL;
	entry->prev = NULL;
}

#if 0
/**
 * list_move - delete from one list and add as another's head
 * @list: the entry to move
 * @head: the head that will precede our entry
 */
static inline void list_move(list_t *list, list_t *head)
{
        __list_del(list->prev, list->next);
        list_add(list, head);
}

/**
 * list_move_tail - delete from one list and add as another's tail
 * @list: the entry to move
 * @head: the head that will follow our entry
 */
static inline void list_move_tail(list_t *list, list_t *head)
{
        __list_del(list->prev, list->next);
        list_add_tail(list, head);
}
#endif


/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-offsetof(type, member)))
//(unsigned long)(&((type *)0)->member)

/**
 * list_foreach	-	iterate over a list
 * @pos:	the &list_t to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_foreach(pos, head) \
	for ((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

/**
 * list_foreach_reverse	-	iterate over a list backwards
 * @pos:	the &list_t to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_foreach_reverse(pos, head) \
	for ((pos) = (head)->prev; (pos) != (head); (pos) = (pos)->prev)
        	
/**
 * list_for_each_safe	-	iterate over a list safe against removal of list entry
 * @pos:	the &list_t to use as a loop counter.
 * @n:		another &list_t to use as temporary storage
 * @head:	the head for your list.
 */
#define list_foreach_safe(pos, n, head) \
	for ((pos) = (head)->next, (n) = (pos)->next; (pos) != (head); (pos) = (n), (n) = (pos)->next)

#if 0
/**
 * list_foreach_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_foreach_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 * list_foreach_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop counter.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_foreach_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))
#endif


/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline bool list_isempty(list_t * head)
{
	return head->next == head;
}

static inline size_t list_size(list_t * head)
{
	size_t i = 0;
	list_t * pos = head;

	while ((pos = pos->next) != head)
	{
		i += 1;
	}

	return i;
}


/**
 * list_sort - sorts the given list
 * @head: the list to sort
 * @cmp_f
 */
static inline void list_sort(list_t * head, list_compare_f cmp)
{
	size_t i, j;
	size_t len = list_size(head);

	if (UNLIKELY(list_size(head) <= 1))
	{
		return;
	}

	//simple bubble sort
	// TODO - make better
	for (i=len-1; i>0; --i)
	{
		list_t * pos;
		for (j=0, pos=head->next; j<i; ++j, pos=pos->next)
		{
			list_t * a = pos;
			list_t * b = pos->next;

			if (cmp(a, b) > 0)
			{
				//swap
				list_t * tmp = b->next;

				a->prev->next = b;
				b->next = a;
				a->next = tmp;

				b->prev = a->prev;
				a->prev = b;
				tmp->prev = a;

				//correct pos
				pos = b;
			}
		}
	}
}

#endif
