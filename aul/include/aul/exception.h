#ifndef __AUL_ERROR_H
#define __AUL_ERROR_H

#include <stdarg.h>
#include <errno.h>

#include <aul/common.h>
#include <aul/string.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct
{
	int code;
	const char message[AUL_STRING_MAXLEN];
} exception_t;

exception_t * exception_new(int code, const char * fmt, ...) CHECK_PRINTF(2,3);
exception_t * exception_vnew(int code, const char * fmt, va_list args);
void exception_clear(exception_t * err);

void exception_make(exception_t * err, int code, const char * fmt, ...) CHECK_PRINTF(3,4);
void exception_vmake(exception_t * err, int code, const char * fmt, va_list args);

static inline void exception_set(exception_t ** err, int code, const char * fmt, ...) CHECK_PRINTF(3, 4);
static inline void exception_set(exception_t ** err, int code, const char * fmt, ...)
{
	if (err != NULL)
	{
		va_list args;
		va_start(args, fmt);
		*err = exception_vnew(code, fmt, args);
		va_end(args);
	}
}

// TODO - make #define??
static inline bool exception_check(exception_t ** err)
{
	return ((err) != NULL && *(err) != NULL && (*(err))->code != 0);
}

static inline void exception_free(exception_t * err)
{
	if (err != NULL) free(err);
}


#ifdef __cplusplus
}
#endif
#endif
