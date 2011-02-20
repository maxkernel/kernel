#ifndef __AUL_ERROR_H
#define __AUL_ERROR_H

#include <stdarg.h>
#include <aul/common.h>
#include <aul/string.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct
{
	int code;
	string_t message;
} exception_t;

#define exception_check(e)		(e != NULL && *e != NULL && (*e)->code != 0)

exception_t * exception_new(int code, const char * fmt, ...) CHECK_PRINTF(2,3);
void exception_clear(exception_t * err);

void exception_make(exception_t * err, int code, const char * fmt, ...) CHECK_PRINTF(3,4);
void exception_vmake(exception_t * err, int code, const char * fmt, va_list args);

#define exception_free(err)		FREE(err)


#ifdef __cplusplus
}
#endif
#endif
