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
	String message;
} Error;

#define error_check(e)		(e != NULL && *e != NULL && (*e)->code != 0)

Error * error_new(int code, const char * fmt, ...);
void error_clear(Error * err);

#define error_free(err)		FREE(err)


#ifdef __cplusplus
}
#endif
#endif
