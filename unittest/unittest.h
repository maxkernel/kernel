#ifndef __UNIT_TEST_H
#define __UNIT_TEST_H

#include <stdbool.h>


#define LINEBUF		256

#ifdef __cplusplus
extern "C" {
#endif


void module(const char * modname);

#undef assert
#define assert(val, desc) __assert((val), (#val), desc)
void __assert(bool val, const char * valstr, const char * desc);



#ifdef __cplusplus
}
#endif
#endif
