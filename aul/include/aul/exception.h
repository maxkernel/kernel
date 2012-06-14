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

exception_t * exception_new(int code, const char * fmt, ...) check_printf(2,3);
exception_t * exception_vnew(int code, const char * fmt, va_list args);

void exception_make(exception_t * err, int code, const char * fmt, ...) check_printf(3,4);
void exception_vmake(exception_t * err, int code, const char * fmt, va_list args);

static inline void exception_set(exception_t ** err, int code, const char * fmt, ...) check_printf(3, 4);
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

#define exception_clear(e)		(memset(e, 0, sizeof(exception_t)))
#define exception_code(e)		(((e) == NULL)? 0 : (e)->code)
#define exception_message(e)	(((e) == NULL)? "Unknown error" : (e)->message)

static inline bool exception_check(exception_t ** err)	{ return ((err) != NULL && *(err) != NULL && (*(err))->code != 0); }
#define exception_free(e)		({ if ((e) != NULL) { free(e); (e) = NULL; }})
//static inline void exception_free(exception_t * err) { if (err != NULL) free(err); }

#ifdef __cplusplus
}
#endif
#endif
