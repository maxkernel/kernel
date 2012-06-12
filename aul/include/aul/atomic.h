#ifndef __AUL_ATOMIC_H
#define __AUL_ATOMIC_H


#define atomic_add(variable, amount)		(__sync_add_and_fetch(&(variable), (amount)))
#define atomic_sub(variable, amount)		(__sync_sub_and_fetch(&(variable), (amount)))

#define atomic_inc(variable)				atomic_add((variable), 1)
#define atomic_dec(variable)				atomic_sub((variable), 1)

#endif
