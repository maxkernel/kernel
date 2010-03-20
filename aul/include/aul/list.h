#ifndef __AUL_LIST_H
#define __AUL_LIST_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __List
{
	void * data;
	struct __List * next;
} List;

typedef int (*compare_f)(const void * a, const void * b);

#define list_length(list)	list_size(list)
size_t list_size(List * list);
ssize_t list_indexof(List * list, const void * data);
List * list_get(List * list, size_t index);
List * list_find(List * list, const void * data, compare_f comp);

List * list_append(List * list, void * data);
void list_sort(List * list, compare_f comp);

List * list_tail(List * list);
void list_free(List * list);

#ifdef __cplusplus
}
#endif
#endif
