#ifndef __MAX_MODEL_H
#define __MAX_MODEL_H

#include <inttypes.h>
#include <stdbool.h>

#include <aul/common.h>
#include <aul/exception.h>
#include <aul/string.h>
#include <maxmeta.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODEL_SIZE_PATH					META_SIZE_PATH
#define MODEL_SIZE_NAME					MAX(META_SIZE_ANNOTATE, MAX(META_SIZE_FUNCTION, MAX(META_SIZE_VARIABLE, META_SIZE_BLOCKNAME)))
#define MODEL_SIZE_SHORTDESCRIPTION		META_SIZE_SHORTDESCRIPTION
#define MODEL_SIZE_CONSTRAINT			25
#define MODEL_SIZE_BLOCKIONAME			(MODEL_SIZE_NAME + 6)	// Support array indexes on end of name (ex. '[25]')

#define MODEL_MAX_IDS					300  //TODO - make this/these driven by requirements!
#define MODEL_MAX_ARGS					10	// TODO - find a reasonable value for this
#define MODEL_MAX_SCRIPTS				20							// Maximum number of scripts per model
#define MODEL_MAX_MODULES				10							// Maximum number of modules per script
#define MODEL_MAX_CONFIGPARAMS			META_MAX_CONFIGPARAMS		// Maximum number of set config params per module
#define MODEL_MAX_LINKABLES				(25 /* Blockinsts */ + 25 /* Syscalls */ + 10 /* Rategroups */ )		// Maximum number of block instances per script
#define MODEL_MAX_LINKS					(MODEL_MAX_LINKABLES * 20)	// Maximum number of links per script
#define MODEL_MAX_RATEGROUPELEMS		15							// Maximum number of block instances registered to a rategroup


typedef uint32_t mid_t;
typedef void (*model_analyse_f)();
typedef void (*model_destroy_f)();

typedef enum
{
	model_none			= (0x0),
	model_script		= (0x1 << 0),
	model_module		= (0x1 << 1),
	model_configparam	= (0x1 << 2),
	model_blockinst		= (0x1 << 3),
	model_syscall		= (0x1 << 4),
	model_rategroup		= (0x1 << 5),
	model_link			= (0x1 << 6),
} modeltype_t;

#define model_linkable		(model_blockinst | model_syscall | model_rategroup)


typedef struct
{
	mid_t id;
	modeltype_t type;
	model_destroy_f destroy;
	model_analyse_f analyse;
	void * userdata;
} modelhead_t;

typedef struct
{
	modelhead_t head;

	char name[MODEL_SIZE_NAME];
	char sig;
	char constraint[MODEL_SIZE_CONSTRAINT];
	void * value;
} model_configparam_t;

typedef struct
{
	modelhead_t head;

	meta_t * meta;
	char path[MODEL_SIZE_PATH];
	char name[MODEL_SIZE_NAME];
	version_t version;

	model_configparam_t * configparams[MODEL_MAX_CONFIGPARAMS];
} model_module_t;

typedef struct
{
	char name[MODEL_SIZE_NAME];
	char description[MODEL_SIZE_SHORTDESCRIPTION];
} model_syscall_t;

typedef struct
{
	char name[MODEL_SIZE_NAME];
	model_module_t * module;
	char * args[MODEL_MAX_ARGS];
} model_blockinst_t;

typedef struct
{
	char name[MODEL_SIZE_NAME];
	double hertz;
	model_blockinst_t * blocks[MODEL_MAX_RATEGROUPELEMS];
} model_rategroup_t;

typedef struct __model_linkable_t
{
	modelhead_t head;

	union
	{
		model_blockinst_t * blockinst;
		model_syscall_t * syscall;
		model_rategroup_t * rategroup;
	} backing;
} model_linkable_t;

typedef struct
{
	modelhead_t head;

	char out_name[MODEL_SIZE_BLOCKIONAME];
	char in_name[MODEL_SIZE_BLOCKIONAME];
	model_linkable_t * out;
	model_linkable_t * in;
} model_link_t;

typedef struct
{
	modelhead_t head;

	char path[MODEL_SIZE_PATH];
	model_module_t * modules[MODEL_MAX_MODULES];
	model_linkable_t * linkables[MODEL_MAX_LINKABLES];
	model_link_t * links[MODEL_MAX_LINKS];
} model_script_t;


typedef struct
{
	mid_t numids;
	modelhead_t * id[MODEL_MAX_IDS];

	model_script_t * scripts[MODEL_MAX_SCRIPTS];
	model_module_t * modules[MODEL_MAX_MODULES * MODEL_MAX_SCRIPTS];

	// House keeping things
	size_t malloc_size;
} model_t;

typedef struct
{
	//void * (*links)()
} model_analysis_t;

#define model_foreach(item, items, maxsize) \
	for (size_t __i = 0; ((item) = (items)[__i]) != NULL && __i < (maxsize); __i++)

model_t * model_new();
void * model_setuserdata(modelhead_t * head, void * newuserdata);
void model_free(model_t * model);

string_t model_getconstraint(const char * desc);
string_t model_getbase(const char * ioname);
string_t model_getsubscript(const char * ioname);

model_script_t * model_script_new(model_t * model, const char * path, exception_t ** err);

model_module_t * model_module_add(model_t * model, model_script_t * script, meta_t * meta, exception_t ** err);
bool model_module_setconfig(model_t * model, model_module_t * module, const char * configname, const char * value, exception_t ** err);

model_linkable_t * model_blockinst_new(model_t * model, model_module_t * module, model_script_t * script, const char * blockname, const char ** args, size_t args_length, exception_t ** err);
model_linkable_t * model_rategroup_new(model_t * model, model_script_t * script, const char * name, double hertz, const model_linkable_t ** elems, size_t elems_length, exception_t ** err);
model_linkable_t * model_syscall_new(model_t * model, model_script_t * script, const char * funcname, const char * desc, exception_t ** err);
model_link_t * model_link_new(model_t * model, model_script_t * script, model_linkable_t * outinst, const char * outname, model_linkable_t * ininst, const char * inname, exception_t ** err);



#ifdef __cplusplus
}
#endif
#endif
