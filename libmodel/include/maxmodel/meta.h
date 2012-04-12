#ifndef __MAXMODEL_META_H
#define __MAXMODEL_META_H

#include <stdint.h>
#include <stdbool.h>

#include <aul/common.h>
#include <aul/exception.h>
#include <aul/version.h>
#include <aul/iterator.h>

#ifdef __cplusplus
extern "C" {
#endif

#define META_SIZE_PATH					1024
#define META_SIZE_ANNOTATE				50
#define META_SIZE_DEPENDENCY			50
#define META_SIZE_LONGDESCRIPTION		200
#define META_SIZE_SHORTDESCRIPTION		120
#define META_SIZE_BLOCKNAME				50
#define META_SIZE_BLOCKIONAME			25
#define META_SIZE_FUNCTION				50
#define META_SIZE_VARIABLE				50
#define META_SIZE_SIGNATURE				25

#define META_MAX_DEPENDENCIES			15
#define META_MAX_SYSCALLS				50
#define META_MAX_CONFIGS				50
//#define META_MAX_CALPARAMS				50
#define META_MAX_BLOCKS					25
#define META_MAX_BLOCKCBS				(2 * META_MAX_BLOCKS)
#define META_MAX_BLOCKIOS				(2 * 30 * META_MAX_BLOCKS)
#define META_MAX_STRUCTS 				\
	/* Maximum number of structs in meta section */ \
	( 1		/* meta_begin */		\
	+ 1		/* meta_name */			\
	+ 1		/* meta_version */		\
	+ 1		/* meta_author */		\
	+ 1		/* meta_description */	\
	+ META_MAX_DEPENDENCIES			\
	+ 1		/* meta_init */			\
	+ 1		/* meta_destroy */		\
	+ 1		/* meta_preactivate */	\
	+ 1		/* meta_postactivate */	\
	+ META_MAX_SYSCALLS				\
	+ META_MAX_CONFIGS				\
	/*+ META_MAX_CALPARAMS*/		\
	+ 1		/* meta_modechange */	\
	+ 1		/* meta_preview */		\
	+ META_MAX_BLOCKS				\
	+ META_MAX_BLOCKCBS				\
	+ META_MAX_BLOCKIOS				\
	)


#define __meta_special					0x31415926
#define __meta_a1						__attribute__((aligned(1)))


typedef void * (*meta_callback_f)();
typedef void (*meta_callback_vv_f)();
typedef void (*meta_callback_vi_f)(int);
typedef void (*meta_callback_vp_f)(void *);
typedef bool (*meta_callback_bv_f)();
typedef bool (*meta_callback_bscpp_f)(const char *, char, void *, void *);
typedef void * meta_variable_m;

typedef enum
{
	meta_none			= 0x00,
	meta_begin			= 0x01,

	meta_name			= 0x10,
	meta_version,
	meta_author,
	meta_description,
	meta_dependency,

	meta_oninit			= 0x20,
	meta_ondestroy,
	meta_onpreact,
	meta_onpostact,

	meta_syscall		= 0x30,

	meta_config			= 0x40,

	//meta_calparam		= 0x50,
	//meta_calonmodechange,
	//meta_calonpreview,

	meta_block			= 0x60,
	meta_blockonupdate,
	meta_blockondestroy,
	meta_blockinput,
	meta_blockoutput,

} metatype_t;

typedef enum
{
	meta_unknownio		= '?',
	meta_input			= 'I',
	meta_output			= 'O',
} metaiotype_t;

typedef struct
{
	const size_t size __meta_a1;
	const metatype_t type __meta_a1;
} metahead_t;


typedef struct
{
	const metahead_t head __meta_a1;
	const uint32_t special __meta_a1;
} meta_begin_t;

typedef struct
{
	const metahead_t head __meta_a1;
	const char name[META_SIZE_ANNOTATE] __meta_a1;
} meta_annotate_t;

typedef struct
{
	const metahead_t head __meta_a1;
	const version_t version __meta_a1;
} meta_version_t;

typedef struct
{
	const metahead_t head __meta_a1;
	const char description[META_SIZE_LONGDESCRIPTION] __meta_a1;
} meta_description_t;

typedef struct
{
	const metahead_t head __meta_a1;
	const char dependency[META_SIZE_DEPENDENCY] __meta_a1;
} meta_dependency_t;

typedef struct
{
	const metahead_t head __meta_a1;
	const char function_name[META_SIZE_FUNCTION] __meta_a1;
	const char function_signature[META_SIZE_SIGNATURE] __meta_a1;
	const char function_description[META_SIZE_LONGDESCRIPTION] __meta_a1;
} meta_callback_head_t;

typedef struct
{
	meta_callback_head_t callback __meta_a1;
	meta_callback_f function __meta_a1;
} meta_callback_t;

typedef struct
{
	meta_callback_head_t callback __meta_a1;
	meta_callback_vv_f function __meta_a1;
} meta_callback_vv_t;

typedef struct
{
	meta_callback_head_t callback __meta_a1;
	meta_callback_vi_f function __meta_a1;
} meta_callback_vi_t;

typedef struct
{
	meta_callback_head_t callback __meta_a1;
	meta_callback_bv_f function __meta_a1;
} meta_callback_bv_t;

typedef struct
{
	meta_callback_head_t callback __meta_a1;
	meta_callback_bscpp_f function __meta_a1;
} meta_callback_bscpp_t;

typedef struct
{
	const metahead_t head __meta_a1;
	const char variable_name[META_SIZE_VARIABLE] __meta_a1;
	const char variable_signature __meta_a1;
	const char variable_description[META_SIZE_SHORTDESCRIPTION] __meta_a1;
	const meta_variable_m variable __meta_a1;
} meta_variable_t;

typedef struct
{
	const metahead_t head __meta_a1;
	const char block_name[META_SIZE_BLOCKNAME] __meta_a1;
	const char block_description[META_SIZE_LONGDESCRIPTION] __meta_a1;
	const char constructor_name[META_SIZE_FUNCTION] __meta_a1;
	const char constructor_signature[META_SIZE_SIGNATURE] __meta_a1;
	const char constructor_description[META_SIZE_LONGDESCRIPTION] __meta_a1;
	const meta_callback_f constructor __meta_a1;
} meta_block_t;

typedef struct
{
	meta_callback_head_t callback __meta_a1;
	const char blockname[META_SIZE_BLOCKNAME] __meta_a1;
	meta_callback_vp_f function __meta_a1;
} meta_blockcallback_t;

typedef struct
{
	const metahead_t head __meta_a1;
	const char blockname[META_SIZE_BLOCKNAME] __meta_a1;
	const char io_name[META_SIZE_BLOCKIONAME] __meta_a1;
	const char io_signature __meta_a1;
	const char io_description[META_SIZE_SHORTDESCRIPTION] __meta_a1;
} meta_blockio_t;


typedef void (*__meta_begin_callback)(const meta_begin_t * begin);

#define ___meta_name(a, b, c)					__ ## a ## ___ ## b ## _ ## c
#define __meta_name(a, b, c)					___meta_name(a, b, c)
#define __meta_head(s, t)						{ (s), (t) }
#define __meta_intro(type, name)				static const type name __attribute__((section(".meta"),unused,aligned(1)))
#define __meta_unique(type, code)				__meta_intro(type, __meta_name(code,__LINE__, __COUNTER__))
#define __meta_begin()							__meta_intro(meta_begin_t, __meta_begin_here) = { __meta_head(sizeof(meta_begin_t), meta_begin), __meta_special }
#define __meta_constructor						__attribute__((constructor)) static void __meta_name(constructor,__LINE__, __COUNTER__) () \
													{ extern __meta_begin_callback __meta_init; if (__meta_init != NULL) __meta_init(&__meta_begin_here); }
#define __meta_write(type, code, ...)			__meta_unique(type, code) = { __meta_head(sizeof(type), code), __VA_ARGS__ }
#define __meta_cbwrite(type, code, a,b,c, ...)	__meta_unique(type, code) = {{ __meta_head(sizeof(type), code), a,b,c }, __VA_ARGS__ }

#define meta_sizemax \
	(max(sizeof(meta_begin_t),				\
	 max(sizeof(meta_annotate_t),			\
	 max(sizeof(meta_version_t),			\
	 max(sizeof(meta_description_t),		\
	 max(sizeof(meta_dependency_t),			\
	 max(sizeof(meta_callback_t),			\
	 max(sizeof(meta_variable_t),			\
	 max(sizeof(meta_block_t),				\
	 max(sizeof(meta_blockcallback_t),		\
	 sizeof(meta_blockio_t)					\
	 ))))))))))

#define meta_sizeof(_) \
	 ((_)== meta_begin? sizeof(meta_begin_t) : (_)== meta_name? sizeof(meta_annotation_t) : (_)== meta_version? sizeof(meta_version_t) \
	: (_)== meta_author? sizeof(meta_annotate_t) : (_)== meta_description? sizeof(meta_decription_t) : (_)== meta_dependency? sizeof(meta_dependency_t) \
	: (_)== meta_init? sizeof(meta_callback_bv_t) : (_)== meta_destroy? sizeof(meta_callback_vv_t) : (_)== meta_preactivate? sizeof(meta_callback_vv_t)) \
	: (_)== meta_postactivate? sizeof(meta_callback_vv_t) : (_)== meta_syscall? sizeof(meta_callback_t) : (_)== meta_configparam? sizeof(meta_variable_t) \
	: (_)== meta_calparam? sizeof(meta_variable_t) : (_)== meta_calmodechange? sizeof(meta_callback_vi_t) : (_)== meta_calpreview? sizeof(meta_callback_bscpp_t) \
	: (_)== meta_block? sizeof(meta_block_t) : (_)== meta_blockupdate? sizeof(meta_blockcallback_t) : (_)== meta_blockdestroy? sizeof(meta_blockcallback_t) \
	: (_)== meta_blockinput? sizeof(meta_blockio_t) : (_)== meta_blockoutput? sizeof(meta_blockio_t) : 0)



#define module_name(name)						__meta_begin(); \
												__meta_constructor \
												__meta_write(meta_annotate_t, meta_name, (name))
#define module_version(major, minor, revision)	__meta_write(meta_version_t, meta_version, version(major, minor, revision))
#define module_author(author)					__meta_write(meta_annotate_t, meta_author, (author))
#define module_description(description)			__meta_write(meta_description_t, meta_description, (description))
#define module_dependency(dependency)			__meta_write(meta_dependency_t, meta_dependency, (dependency))

#define module_oninitialize(function)			__meta_cbwrite(meta_callback_bv_t, meta_oninit, (#function), "b:v", "", (function))
#define module_ondestroy(function)				__meta_cbwrite(meta_callback_vv_t, meta_ondestroy, (#function), "v:v", "", (function))
#define module_onpreactivate(function)			__meta_cbwrite(meta_callback_vv_t, meta_onpreact, (#function), "v:v", "", (function))
#define module_onpostactivate(function)			__meta_cbwrite(meta_callback_vv_t, meta_onpostact, (#function), "v:v", "", (function))
#define module_config(variable, sig, desc)		__meta_write(meta_variable_t, meta_config, (#variable), (sig), (desc), (meta_variable_m)&(variable))

#define define_syscall(function, sig, desc)		__meta_cbwrite(meta_callback_t, meta_syscall, (#function), (sig), (desc), (meta_callback_f)(function))

//#define cal_param(variable, sig, desc)			__meta_write(meta_variable_t, meta_calparam, (#variable), (sig), (desc), (meta_variable_m)&(variable))
//#define cal_onmodechange(function)				__meta_cbwrite(meta_callback_vi_t, meta_calonmodechange, (#function), "v:i", "", (function))
//#define cal_onpreview(function)					__meta_cbwrite(meta_callback_bscpp_t, meta_calonpreview, (#function), "b:scpp", "", (function))

#define define_block(blockname, block_desc, constructor, sig, constructor_desc) \
												__meta_write(meta_block_t, meta_block, (#blockname), (block_desc), (#constructor), (sig), (constructor_desc), (meta_callback_f)(constructor))
#define block_onupdate(blockname, function)		__meta_cbwrite(meta_blockcallback_t, meta_blockonupdate, (#function), "v:p", "", (#blockname), (function))
#define block_ondestroy(blockname, function)	__meta_cbwrite(meta_blockcallback_t, meta_blockondestroy, (#function), "v:p", "", (#blockname), (function))
#define block_input(blockname, input_name, sig, desc) \
												__meta_write(meta_blockio_t, meta_blockinput, (#blockname), (#input_name), (sig), (desc))
#define block_output(blockname, output_name, sig, desc) \
												__meta_write(meta_blockio_t, meta_blockoutput, (#blockname), (#output_name), (sig), (desc))

#define __meta_struct_version		version(1,0,0)

typedef struct
{
	version_t meta_version;
	size_t section_size;

	bool resolved;
	void * dlobject;

	char path[META_SIZE_PATH];
	uint8_t * buffer;
	uint8_t buffer_layout[META_MAX_STRUCTS];
	void * buffer_indexes[META_MAX_STRUCTS];

	meta_begin_t * begin;
	meta_annotate_t * name;
	meta_version_t * version;
	meta_annotate_t * author;
	meta_description_t * description;
	meta_dependency_t * dependencies[META_MAX_DEPENDENCIES];

	meta_callback_bv_t * init;
	meta_callback_vv_t * destroy;
	meta_callback_vv_t * preact;
	meta_callback_vv_t * postact;

	meta_callback_t * syscalls[META_MAX_SYSCALLS];

	meta_variable_t * configs[META_MAX_CONFIGS];

	//meta_callback_vi_t * cal_modechange;
	//meta_callback_bscpp_t * cal_preview;
	//meta_variable_t * cal_params[META_MAX_CALPARAMS];

	meta_block_t * blocks[META_MAX_BLOCKS];
	meta_blockcallback_t * block_callbacks[META_MAX_BLOCKCBS];
	meta_blockio_t * block_ios[META_MAX_BLOCKIOS];
} meta_t;

typedef meta_callback_bv_f meta_initializer;
typedef meta_callback_vv_f meta_destroyer;
typedef meta_callback_vv_f meta_activator;

#define meta_foreach(item, items, maxsize) \
	for (size_t __i = 0; ((item) = (items)[__i]) != NULL && __i < (maxsize); __i++)

#if defined(USE_BFD)
meta_t * meta_parseelf(const char * path, exception_t ** err);
#endif

#if defined(USE_DL)
bool meta_loadmodule(meta_t * meta, exception_t ** err);
#endif

meta_t * meta_parsebase64(const char * from, size_t length, exception_t ** err);
size_t meta_encodebase64(meta_t * meta, const char * to, size_t length);

meta_t * meta_copy(const meta_t * meta);
void meta_destroy(meta_t * meta);

void meta_getinfo(const meta_t * meta, const char ** path, const char ** name, const version_t ** version, const char ** author, const char ** desc);
void meta_getactivators(const meta_t * meta, meta_initializer * init, meta_destroyer * destroy, meta_activator * preact, meta_activator * postact);

bool meta_getblock(const meta_t * meta, const char * blockname, char const ** constructor_sig, size_t * ios_length, const char ** desc);
bool meta_getblockios(const meta_t * meta, const char * blockname, char const ** names, metaiotype_t * types, char * sigs, const char ** descs, size_t length);

iterator_t meta_dependencyitr(const meta_t * meta);
bool meta_dependencynext(iterator_t itr, const char ** dependency);

bool meta_lookupconfig(const meta_t * meta, const char * configname, const meta_variable_t ** config);
iterator_t meta_configitr(const meta_t * meta);
bool meta_confignext(iterator_t itr, const meta_variable_t ** config);

bool meta_lookupsyscall(const meta_t * meta, const char * syscallname, const meta_callback_t ** syscall);
iterator_t meta_syscallitr(const meta_t * meta);
bool meta_syscallnext(iterator_t itr, const meta_callback_t ** syscall);

void meta_getvariable(const meta_variable_t * variable, const char ** name, char * sig, const char ** desc, meta_variable_m * value);
void meta_getcallback(const meta_callback_t * callback, const char ** name, const char ** sig, const char ** desc, meta_callback_f * value);

#ifdef __cplusplus
}
#endif
#endif
