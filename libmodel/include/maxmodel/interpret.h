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
} interpret_callbacks;

#ifdef USE_LUA
bool interpret_lua(model_t * model, const char * path, const interpret_callbacks * cbs, exception_t ** err);
#endif

#ifdef __cplusplus
}
#endif
#endif
