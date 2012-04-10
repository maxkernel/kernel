#ifndef __AUL_CONSTRAINT_H
#define __AUL_CONSTRAINT_H

#include <inttypes.h>

#include <aul/common.h>
#include <aul/exception.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUL_CONSTRAINT_TEXTSIZE			35
#define AUL_CONSTRAINT_PRIVDATASIZE		50		// Careful when changing this value. A value too small will *CRASH* the process!

typedef bool (*ctest_f)(const void * object, const char * value, char * hint, size_t hint_length);
typedef bool (*cstep_f)(const void * object, double * step);

typedef struct
{
	char text[AUL_CONSTRAINT_TEXTSIZE];
	struct {
		ctest_f test;
		cstep_f step;
	} cbs;
	uint8_t privdata[AUL_CONSTRAINT_PRIVDATASIZE];
} constraint_t;


constraint_t constraint_new(const char * constraint, exception_t ** err);
bool constraint_check(const constraint_t * constraint, const char * value, char * hint, size_t hint_length);
bool constraint_getstep(const constraint_t * constraint, double * step);

constraint_t constraint_parse(const char * string, exception_t ** err);
const char * constraint_parsepast(const char * string);

#ifdef __cplusplus
}
#endif
#endif
