#ifndef __AUL_STRING_H
#define __AUL_STRING_H

#include <stdarg.h>
#include <inttypes.h>
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


string_t string_blank();
string_t string_new(const char * fmt, ...) check_printf(1, 2);
void string_set(string_t * str, const char * fmt, ...) check_printf(2, 3);
void string_vset(string_t * str, const char * fmt, va_list args);
void string_append(string_t * str, const char * fmt, ...) check_printf(2, 3);
void string_vappend(string_t * str, const char * fmt, va_list args);

size_t string_available(const string_t * str);
string_t string_clone(const string_t * str);
char * string_copy(const string_t * str);		// TODO - remove string_copy (use strdup)
void string_clear(string_t * str);


// String utils
#define strprefix(string, prefix)	(strlen(string) >= strlen(prefix) && strncmp(string, prefix, strlen(prefix)) == 0)
#define strsuffix(string, suffix)	(strlen(string) >= strlen(suffix) && strcmp(string + (strlen(string) - strlen(suffix)), suffix) == 0)


// Serializa utils
#define ser_bool(b)			(((b) == NULL)? "(undefined)" : ((b)? "True" : "False"))
#define ser_string(s)		(((s) == NULL || strlen(s) == 0)? "(undefined)" : (s))
#define ser_version(v)		(((v) == NULL)? "(undefined)" : version_tostring(*(v)).string)



#ifdef __cplusplus
}
#endif
#endif
