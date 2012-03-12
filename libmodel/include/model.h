#ifndef __MAX_MODEL_H
#define __MAX_MODEL_H

#include <inttypes.h>
#include <stdbool.h>

#include <aul/exception.h>
#include <maxmeta.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODEL_SIZE_PATH				1024
#define MODEL_SIZE_BLOCKNAME		50

#define MODEL_MAX_IDS				300
#define MODEL_MAX_SCRIPTS			20		// Maximum number of scripts per model
#define MODEL_MAX_MODULES			10		// Maximum number of modules per script

#define __model_a1					__attribute__((aligned(1)))

typedef uint32_t mid_t;
typedef void (*model_analyse_f)();
typedef void (*model_destroy_f)();

typedef enum
{
	model_none			= 0x00,

	model_script		= 0x10,

	model_module		= 0x20,
} modeltype_t;

typedef struct
{
	mid_t id;
	modeltype_t type;
	model_destroy_f destroy;
	void * userdata;
} modelhead_t;

typedef struct
{
	modelhead_t head;

	char name[MODEL_SIZE_BLOCKNAME];
} model_blockinst_t;

typedef struct
{
	modelhead_t head;

	meta_t * meta;
	//char path[MODEL_SIZE_PATH];	// TODO - Uncomment these prior to release and
	//double version;				// uncomment the corresponding memset in model_script_addmodule, etc...
} model_module_t;

typedef struct
{
	modelhead_t head;

	char path[MODEL_SIZE_PATH];
	model_module_t * modules[MODEL_MAX_MODULES];
} model_script_t;


typedef struct
{
	mid_t numids;
	modelhead_t * id[MODEL_MAX_IDS];

	model_script_t * scripts[MODEL_MAX_SCRIPTS];
	model_module_t * modules[MODEL_MAX_MODULES * MODEL_MAX_SCRIPTS];
} model_t;


model_t * model_new();
void model_free(model_t * model);

model_script_t * model_script_newscript_withpath(model_t * model, const char * path, exception_t ** err);
model_module_t * model_script_addmodule_frommeta(model_t * model, model_script_t * script, meta_t * meta, exception_t ** err);

model_blockinst_t * model_module_newblockinst(model_t * model, model_module_t * module, const char * blockname, exception_t ** err);

#ifdef __cplusplus
}
#endif
#endif
