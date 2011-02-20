#include <stdlib.h>
#include <string.h>
#include <aul/common.h>
#include <aul/exception.h>

exception_t * exception_new(int code, const char * fmt, ...)
{
	exception_t * err = malloc0(sizeof(exception_t));
	exception_clear(err);

	va_list args;
	va_start(args, fmt);
	exception_vmake(err, code, fmt, args);
	va_end(args);

	return err;
}

void exception_clear(exception_t * err)
{
	err->code = 0;
	string_clear(&err->message);
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
	string_vappend(&err->message, fmt, args);
}


