#include <stdlib.h>
#include <string.h>
#include <aul/error.h>

Error * error_new(int code, const char * fmt, ...)
{
	Error * err = malloc(sizeof(Error));
	memset(err, 0, sizeof(Error));
	
	err->code = code;
	
	va_list args;
	va_start(args, fmt);
	string_vappend(&err->message, fmt, args);
	va_end(args);
	
	return err;
}

boolean error_isset(Error * err)
{
	return err->code != 0;
}

void error_clear(Error * err)
{
	err->code = 0;
	string_clear(&err->message);
}


