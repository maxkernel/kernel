#include <stdio.h>
#include <stdlib.h>

#include <aul/common.h>
#include <aul/string.h>

string_t string_blank()
{
	string_t str;
	str.string[0] = 0;
	str.length = 0;

	return str;
}

string_t string_new(const char * fmt, ...)
{
	string_t str = string_blank();
	
	va_list args;
	va_start(args, fmt);
	string_vappend(&str, fmt, args);
	va_end(args);

	return str;
}

void string_set(string_t * str, const char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	string_vset(str, fmt, args);
	va_end(args);
}

void string_vset(string_t * str, const char * fmt, va_list args)
{
	string_clear(str);
	string_vappend(str, fmt, args);
}

void string_append(string_t * str, const char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	string_vappend(str, fmt, args);
	va_end(args);
}

void string_vappend(string_t * str, const char * fmt, va_list args)
{
	str->length += vsnprintf(str->string + str->length, AUL_STRING_MAXLEN - str->length, fmt, args);
}

size_t string_available(const string_t * str)
{
	return AUL_STRING_MAXLEN - str->length;
}

string_t string_clone(const string_t * str)
{
	string_t cpy;
	memcpy(&cpy, str, sizeof(string_t));

	return cpy;
}

char * string_copy(const string_t * str)
{
	return strdup(str->string);
}

void string_clear(string_t * str)
{
	str->length = 0;
	str->string[0] = 0;
}

