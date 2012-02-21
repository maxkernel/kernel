#ifndef __AUL_ITERATOR_H
#define __AUL_ITERATOR_H

#include <aul/common.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUL_ITERATOR_MAX_ITRS		128

typedef signed int iterator_t;
typedef const void * (*itrnext_f)(void ** object);
typedef void (*itrfree_f)(void * object);

void iterator_init();
iterator_t iterator_new(itrnext_f next_func, itrfree_f free_func, void * object);
const void * iterator_next(iterator_t itr);
void iterator_free(iterator_t itr);

#ifdef __cplusplus
}
#endif
#endif
