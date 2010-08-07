#include <stdlib.h>
#include <aul/common.h>

#include <aul/list.h>

size_t list_size(List * list)
{
	if (list == NULL)
	{
		return 0;
	}
	
	size_t size = 0;
	while (list != NULL)
	{
		size++;
		list = list->next;
	}
	
	return size;
}

ssize_t list_indexof(List * list, const void * data)
{
	ssize_t index = 0;
	while (list != NULL)
	{
		if (list->data == data)
		{
			return index;
		}
		
		index++;
		list = list->next;
	}
	
	return -1;
}

List * list_get(List * list, size_t index)
{
	while (list != NULL)
	{
		if (index == 0)
		{
			return list;
		}
		
		index--;
		list = list->next;
	}
	
	return NULL;
}

List * list_find(List * list, const void * data, compare_f comp)
{
	while (list != NULL)
	{
		if (comp(data, list->data) == 0)
		{
			return list;
		}
		
		list = list->next;
	}
	
	return NULL;
}

List * list_append(List * list, void * data)
{
	List * tail = list_tail(list);
	
	List * item = malloc(sizeof(List));
	item->data = data;
	item->next = NULL;
	
	if (tail != NULL)
	{
		tail->next = item;
	}
	
	return list != NULL? list : item;
}

void list_sort(List * list, compare_f comp)
{
	size_t i, j;
	size_t len = list_size(list);
	
	if (len <= 1)
	{
		return;
	}
	
	//simple bubble sort
	for (i=len; i>0; i--)
	{
		List * next = list;
		for (j=0; j<i-1; j++)
		{
			void * a = next->data;
			void * b = next->next->data;
			
			if (comp(a, b) > 0)
			{
				//swap
				next->data = b;
				next->next->data = a;
			}
			
			next = next->next;
		}
	}
}

List * list_tail(List * list)
{
	if (list == NULL)
	{
		return NULL;
	}
	
	while (list->next != NULL)
	{
		list = list->next;
	}
	
	return list;
}

void list_free(List * list)
{
	while (list != NULL)
	{
		List * item = list;
		list = list->next;
		FREE(item);
	}
}

