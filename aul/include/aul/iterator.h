#ifndef __AUL_ITERATOR_H
#define __AUL_ITERATOR_H

#include <aul/common.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUL_ITERATOR_MAX_ITRS		128

typedef signed int iterator_t;
typedef const void * (*itrnext_f)(const void * object, void ** itrobject);
typedef void (*itrfree_f)(const void * object, void * itrobject);

void iterator_init();
iterator_t iterator_new(const char * class, itrnext_f next_func, itrfree_f free_func, const void * object, void * itrobject);
const void * iterator_next(iterator_t itr, const char * class);
void iterator_free(iterator_t itr);

#ifdef __cplusplus
}
#endif
#endif
