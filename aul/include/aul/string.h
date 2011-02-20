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
} string_t;

#define STRING_INIT				{{0},0}

string_t string_blank();
string_t string_new(const char * fmt, ...) CHECK_PRINTF(1, 2);
void string_append(string_t * str, const char * fmt, ...) CHECK_PRINTF(2, 3);
void string_vappend(string_t * str, const char * fmt, va_list args);

string_t string_clone(const string_t * str);
const char * string_copy(const string_t * str);
void string_clear(string_t * str);


//string utils
#define strprefix(string, prefix)	(strncmp(string, prefix, strlen(prefix)) == 0 && strlen(string) >= strlen(prefix))
#define strsuffix(string, suffix)	(strcmp(string + (strlen(string) - strlen(suffix)), suffix) == 0)


#ifdef __cplusplus
}
#endif
#endif
