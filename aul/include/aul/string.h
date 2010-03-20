#ifndef __AUL_STRING_H
#define __AUL_STRING_H

#include <stdarg.h>
#include <string.h>
#include <aul/common.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUL_STRING_MAXLEN		250

typedef struct
{
	char string[AUL_STRING_MAXLEN];
	size_t length;
} String;

String string_blank();
String string_new(const char * fmt, ...);
void string_append(String * str, const char * fmt, ...);
void string_vappend(String * str, const char * fmt, va_list args);

String string_clone(const String * str);
const char * string_copy(const String * str);
void string_clear(String * str);


//string utils
#define strprefix(string, prefix)	(strncmp(string, prefix, strlen(prefix)) == 0 && strlen(string) >= strlen(prefix))
#define strsuffix(string, suffix)	(strcmp(string + (strlen(string) - strlen(suffix)), suffix) == 0)


#ifdef __cplusplus
}
#endif
#endif
