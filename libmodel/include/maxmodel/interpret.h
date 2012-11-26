#ifndef __MAXMODEL_INTERPRET_H
#define __MAXMODEL_INTERPRET_H

#include <inttypes.h>

#include <aul/exception.h>
#include <aul/log.h>

#include <maxmodel/meta.h>
#include <maxmodel/model.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	void (*log)(level_t level, const char * message);
	meta_t * (*metalookup)(const char * modulename, exception_t ** err);
	bool (*setpath)(const char * newpath, exception_t ** err);
	bool (*appendpath)(const char * path, exception_t ** err);

} interpret_callbacks;

typedef bool (*interpret_f)(model_t * model, const char * path, const interpret_callbacks * cbs, exception_t ** err);

bool interpret_disabled(model_t * model, const char * path, const interpret_callbacks * cbs, exception_t ** err);

#ifdef USE_LUA
bool interpret_lua(model_t * model, const char * path, const interpret_callbacks * cbs, exception_t ** err);
#endif

#ifdef __cplusplus
}
#endif
#endif
