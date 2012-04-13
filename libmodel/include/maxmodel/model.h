#ifndef __MAXMODEL_MODEL_H
#define __MAXMODEL_MODEL_H

#include <inttypes.h>
#include <stdbool.h>

#include <aul/common.h>
#include <aul/exception.h>
#include <aul/constraint.h>
#include <aul/string.h>
#include <maxmodel/meta.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODEL_SIZE_PATH					META_SIZE_PATH
#define MODEL_SIZE_NAME					max(max(META_SIZE_ANNOTATE, META_SIZE_FUNCTION), max(META_SIZE_VARIABLE, META_SIZE_BLOCKNAME))
#define MODEL_SIZE_SIGNATURE			META_SIZE_SIGNATURE
#define MODEL_SIZE_DESCRIPTION			META_SIZE_SHORTDESCRIPTION
#define MODEL_SIZE_VALUE				150
#define MODEL_SIZE_BLOCKIONAME			(MODEL_SIZE_NAME + 6)	// Support array indexes on end of name (ex. '[25]')
#define MODEL_SIZE_MAX \
	(max(MODEL_SIZE_PATH,			\
	 max(MODEL_SIZE_NAME,			\
	 max(MODEL_SIZE_DESCRIPTION,	\
	 max(MODEL_SIZE_VALUE,			\
	 MODEL_SIZE_BLOCKIONAME			\
	 )))))

#define MODEL_MAX_ARGS					10	// TODO - find a reasonable value for this
#define MODEL_MAX_SCRIPTS				20							// Maximum number of scripts per model
#define MODEL_MAX_MODULES				10							// Maximum number of modules per script
#define MODEL_MAX_BACKINGS				((MODEL_MAX_MODULES * MODEL_MAX_SCRIPTS) + 10)			// Maximum number of meta_t backings
#define MODEL_MAX_DEPENDENCIES			META_MAX_DEPENDENCIES		// Maximum number of dependencies per module
#define MODEL_MAX_CONFIGS				META_MAX_CONFIGS			// Maximum number of set config params per module
#define MODEL_MAX_LINKABLES				(25 /* Blockinsts */ + 25 /* Syscalls */ + 10 /* Rategroups */ )		// Maximum number of block instances per script
#define MODEL_MAX_LINKS					(MODEL_MAX_LINKABLES * 20)	// Maximum number of links per script
#define MODEL_MAX_RATEGROUPELEMS		15							// Maximum number of block instances registered to a rategroup
#define MODEL_MAX_IDS								\
	( 1 ) +											\
	( MODEL_MAX_SCRIPTS ) + 						\
	( 2 * MODEL_MAX_SCRIPTS * MODEL_MAX_MODULES ) + \
	( MODEL_MAX_SCRIPTS * MODEL_MAX_LINKABLES ) +	\
	( MODEL_MAX_SCRIPTS * MODEL_MAX_LINKS ) +		\
	( MODEL_MAX_SCRIPTS * MODEL_MAX_MODULES * MODEL_MAX_CONFIGS )


struct __modelhead_t;
//struct __model_modulebacking_t;
struct __model_linkable_t;
struct __model_t;
struct __model_analysis_t;
enum   __modeltraversal_t;

typedef uint32_t mid_t;
typedef bool (*model_destroy_f)(struct __model_t * model, struct __modelhead_t * self, struct __modelhead_t * target);
//typedef void (*model_analyse_f)(const struct __model_t * model, enum __modeltraversal_t traversal, const struct __model_analysis_t * cbs, void * object);

typedef enum
{
	model_none			= (0x0),
	model_model			= (0x1 << 0),
	model_script		= (0x1 << 1),
	model_module		= (0x1 << 2),
	//model_modulebacking	= (0x1 << 3),
	model_config		= (0x1 << 4),
	model_blockinst		= (0x1 << 5),
	model_syscall		= (0x1 << 6),
	model_rategroup		= (0x1 << 7),
	model_link			= (0x1 << 8),
} modeltype_t;

// TODO - figure out if we should delete this!
#define model_linkable(x)		((x) & (model_blockinst | model_syscall | model_rategroup))

typedef enum __modeltraversal_t
{
	traversal_none = 0,
	traversal_scripts_modules_configs_linkables_links,
} modeltraversal_t;

typedef struct __modelhead_t
{
	mid_t id;
	modeltype_t type;

	model_destroy_f destroy;
	//model_analyse_f analyse;
	void * userdata;
} modelhead_t;

typedef struct
{
	modelhead_t head;

	char name[MODEL_SIZE_NAME];
	char sig;
	constraint_t constraints;
	char value[MODEL_SIZE_VALUE];
} model_config_t;

typedef struct
{
	modelhead_t head;

	meta_t * backing;
	struct __model_linkable_t * staticinst;
	model_config_t * configs[MODEL_MAX_CONFIGS];
} model_module_t;

typedef struct
{
	char name[MODEL_SIZE_NAME];
	char description[MODEL_SIZE_DESCRIPTION];
} model_syscall_t;

typedef struct
{
	char name[MODEL_SIZE_NAME];
	model_module_t * module;
	char args_sig[MODEL_SIZE_SIGNATURE];
	char * args[MODEL_MAX_ARGS];
} model_blockinst_t;

typedef struct
{
	char name[MODEL_SIZE_NAME];
	double hertz;
	const struct __model_linkable_t * blocks[MODEL_MAX_RATEGROUPELEMS];
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
	char name[MODEL_SIZE_NAME];
	ssize_t index;
	const model_linkable_t * linkable;
} model_linksymbol_t;

typedef struct
{
	modelhead_t head;

	model_linksymbol_t out;
	model_linksymbol_t in;
} model_link_t;

typedef struct
{
	modelhead_t head;

	char path[MODEL_SIZE_PATH];
	model_module_t * modules[MODEL_MAX_MODULES];
	model_linkable_t * linkables[MODEL_MAX_LINKABLES];
	model_link_t * links[MODEL_MAX_LINKS];
} model_script_t;


typedef struct __model_t
{
	modelhead_t head;

	mid_t numids;
	modelhead_t * id[MODEL_MAX_IDS];

	model_script_t * scripts[MODEL_MAX_SCRIPTS];
	meta_t * backings[MODEL_MAX_BACKINGS];

	// House keeping things
	size_t malloc_size;
} model_t;

typedef struct __model_analysis_t
{
	void * (*scripts)(void * udata, const model_t * model, const model_script_t * script);
	void * (*modules)(void * udata, const model_t * model, const model_module_t * module, const model_script_t * script);
	void * (*configs)(void * udata, const model_t * model, const model_config_t * config, const model_script_t * script, const model_module_t * module);
	void * (*linkables)(void * udata, const model_t * model, const model_linkable_t * linkable);
	void * (*blockinsts)(void * udata, const model_t * model, const model_linkable_t * blockinst);
	void * (*syscalls)(void * udata, const model_t * model, const model_linkable_t * syscall, const model_script_t * script);
	void * (*rategroups)(void * udata, const model_t * model, const model_linkable_t * rategroup, const model_script_t * script);
	void * (*links)(void * udata, const model_t * model, const model_link_t * link, const model_linksymbol_t * out, const model_linksymbol_t * in);
} model_analysis_t;

//#define model_object(o)		((modelhead_t *)(o))

#define model_foreach(item, items, maxsize) \
	for (size_t __i = 0; ((item) = (items)[__i]) != NULL && __i < (maxsize); __i++)

model_t * model_new();
void * model_setuserdata(modelhead_t * head, void * newuserdata);
void model_clearalluserdata(model_t * model);
void model_destroy(model_t * model);
void model_addmeta(model_t * model, const meta_t * meta, exception_t ** err);

//string_t model_getbase(const char * ioname);
//string_t model_getsubscript(const char * ioname);

model_script_t * model_newscript(model_t * model, const char * path, exception_t ** err);
model_module_t * model_newmodule(model_t * model, model_script_t * script, meta_t * meta, exception_t ** err);
model_config_t * model_newconfig(model_t * model, model_module_t * module, const char * configname, const char * value, exception_t ** err);
model_linkable_t * model_newblockinst(model_t * model, model_module_t * module, model_script_t * script, const char * blockname, const char ** args, size_t args_length, exception_t ** err);
model_linkable_t * model_newrategroup(model_t * model, model_script_t * script, const char * name, double hertz, const model_linkable_t ** elems, size_t elems_length, exception_t ** err);
model_linkable_t * model_newsyscall(model_t * model, model_script_t * script, const char * funcname, const char * desc, exception_t ** err);
model_link_t * model_newlink(model_t * model, model_script_t * script, model_linkable_t * outinst, const char * outname, model_linkable_t * ininst, const char * inname, exception_t ** err);

void model_analyse(model_t * model, modeltraversal_t traversal, const model_analysis_t * funcs);

bool model_getmeta(const model_module_t * module, const meta_t ** meta);

bool model_metalookup(const model_t * model, const char * path, const meta_t ** meta);
bool model_modulelookup(const model_t * model, model_script_t * script, const char * path, model_module_t ** module);

void model_getconfig(const model_config_t * config, const char ** name, char * sig, constraint_t * constraints, const char ** value);

#ifdef __cplusplus
}
#endif
#endif
