#include <stdio.h>
#include <stdlib.h>
#include <aul/common.h>

#include <aul/string.h>

String string_blank()
{
	String str;
	str.string[0] = 0;
	str.length = 0;

	return str;
}

String string_new(const char * fmt, ...)
{
	String str = string_blank();
	
	va_list args;
	va_start(args, fmt);
	string_vappend(&str, fmt, args);
	va_end(args);

	return str;
}

void string_append(String * str, const char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	string_vappend(str, fmt, args);
	va_end(args);
}

void string_vappend(String * str, const char * fmt, va_list args)
{
	str->length += vsnprintf(str->string + str->length, AUL_STRING_MAXLEN - str->length, fmt, args);
}

String string_clone(const String * str)
{
	String cpy;
	memcpy(&cpy, str, sizeof(String));

	return cpy;
}

const char * string_copy(const String * str)
{
	size_t length = str->length + 1;
	char * cpy = malloc(length);
	memcpy(cpy, str->string, length);

	return cpy;
}

void string_clear(String * str)
{
	str->length = 0;
	str->string[0] = 0;
}

