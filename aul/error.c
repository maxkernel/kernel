#include <stdlib.h>
#include <string.h>
#include <aul/error.h>

Error * error_new(int code, const char * fmt, ...)
{
	Error * err = malloc(sizeof(Error));
	PZERO(err, sizeof(Error));
	
	err->code = code;
	err->message = err->__message.string;
	
	va_list args;
	va_start(args, fmt);
	string_vappend(&err->__message, fmt, args);
	va_end(args);
	
	return err;
}

void error_clear(Error * err)
{
	err->code = 0;
	string_clear(&err->__message);
}


