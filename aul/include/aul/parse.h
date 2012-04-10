#ifndef __AUL_PARSE_H
#define __AUL_PARSE_H

#include <aul/common.h>
#include <aul/exception.h>

#ifdef __cplusplus
extern "C" {
#endif

int parse_int(const char * s, exception_t ** err);
double parse_double(const char * s, exception_t ** err);
bool parse_bool(const char * s, exception_t ** err);

#ifdef __cplusplus
}
#endif
#endif
