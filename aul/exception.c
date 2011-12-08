#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <aul/common.h>
#include <aul/exception.h>

exception_t * exception_new(int code, const char * fmt, ...)
{
	exception_t * err;

	va_list args;
	va_start(args, fmt);
	err = exception_vnew(code, fmt, args);
	va_end(args);

	return err;
}

exception_t * exception_vnew(int code, const char * fmt, va_list args)
{
	exception_t * err = malloc0(sizeof(exception_t));
	exception_clear(err);
	exception_vmake(err, code, fmt, args);

	return err;
}

void exception_clear(exception_t * err)
{
	err->code = 0;
	memset((void *)err->message, 0, AUL_STRING_MAXLEN);
}

void exception_make(exception_t * err, int code, const char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	exception_vmake(err, code, fmt, args);
	va_end(args);
}

void exception_vmake(exception_t * err, int code, const char * fmt, va_list args)
{
	err->code = code;
	vsnprintf((char *)err->message, AUL_STRING_MAXLEN, fmt, args);
}


