#ifndef __MAX_META_H
#define __MAX_META_H

//#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <aul/common.h>
#include <aul/exception.h>

#ifdef __cplusplus
extern "C" {
#endif

#define META_SIZE_ANNOTATE				50
#define META_SIZE_DEPENDENCY			50
#define META_SIZE_LONGDESCRIPTION		200
#define META_SIZE_SHORTDESCRIPTION		80
#define META_SIZE_BLOCKNAME				50
#define META_SIZE_BLOCKIONAME			25
#define META_SIZE_FUNCTION				50
#define META_SIZE_VARIABLE				50
#define META_SIZE_SIGNATURE				25

#define META_MAX_DEPENDENCIES			15
#define META_MAX_SYSCALLS				50
#define META_MAX_CONFIGPARAMS			50
#define META_MAX_CALPARAMS				50
#define META_MAX_BLOCKS					25
#define META_MAX_BLOCKIOS				30

#define __meta_a1						__attribute__((aligned(1)))

typedef uint32_t metasize_t;
typedef void * (*meta_callback_f)();
typedef void (*meta_callback_vv_f)();
typedef void (*meta_callback_vi_f)(int);
typedef void (*meta_callback_vp_f)(void *);
typedef bool (*meta_callback_bv_f)();
typedef bool (*meta_callback_bscpp_f)(const char *, char, void *, void *);
typedef void * meta_variable_m;

typedef enum
{
	m_none			= 0x00,

	m_name			= 0x10,
	m_version,
	m_author,
	m_description,
	m_dependency,

	m_init			= 0x20,
	m_destroy,
	m_preactivate,
	m_postactivate,

	m_syscall		= 0x30,

	m_configparam	= 0x40,

	m_calparam		= 0x50,
	m_calmodechange,
	m_calpreview,

	m_block			= 0x60,
	m_blockupdate,
	m_blockdestroy,
	m_blockinput,
	m_blockoutput,

} metatype_t;

typedef enum
{
	m_input			= 'I',
	m_output		= 'O',
} metaiotype_t;

typedef struct
{
	const metasize_t size __meta_a1;
	const metatype_t type __meta_a1;
} metahead_t;


typedef struct
{
	const metahead_t head __meta_a1;
	const char name[META_SIZE_ANNOTATE] __meta_a1;
} meta_annotate_t;

typedef struct
{
	const metahead_t head __meta_a1;
	const double version __meta_a1;
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
	const char function_description[META_SIZE_SHORTDESCRIPTION] __meta_a1;
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
	const char constructor_description[META_SIZE_SHORTDESCRIPTION] __meta_a1;
	const meta_callback_f constructor __meta_a1;
} meta_block_t;

typedef struct
{
	meta_callback_head_t callback __meta_a1;
	const char block_name[META_SIZE_BLOCKNAME] __meta_a1;
	meta_callback_vp_f function __meta_a1;
} meta_blockcallback_t;

typedef struct
{
	const metahead_t head __meta_a1;
	const char block_name[META_SIZE_BLOCKNAME] __meta_a1;
	const char io_name[META_SIZE_BLOCKIONAME] __meta_a1;
	const char io_signature __meta_a1;
	const char io_description[META_SIZE_SHORTDESCRIPTION] __meta_a1;
} meta_blockio_t;


#define ___meta_name(a, b)					__ ## a ## __ ## b
#define __meta_name(a, b)					___meta_name(a, b)
#define __meta_head(s, t)					{ (s), (t) }
#define __meta_intro(type, code)			static const type __meta_name(code,__LINE__) __attribute__((section(".meta"),unused,aligned(1)))
#define __meta_write(type, code, ...)		__meta_intro(type, code) = { __meta_head(sizeof(type), code), __VA_ARGS__ }
#define __meta_cbwrite(type, code, a,b,c, ...)		__meta_intro(type, code) = {{ __meta_head(sizeof(type), code), a,b,c }, __VA_ARGS__ }

// TODO - fill all these in!!
#define meta_sizemax \
	(MAX(sizeof(meta_annotate_t),			\
	 MAX(sizeof(meta_version_t),			\
	 MAX(sizeof(meta_description_t),		\
	 MAX(sizeof(meta_dependency_t),			\
	 MAX(sizeof(meta_callback_t),			\
	 MAX(sizeof(meta_variable_t),			\
	 MAX(sizeof(meta_block_t),				\
	 MAX(sizeof(meta_blockcallback_t),		\
	 sizeof(meta_blockio_t)					\
	 )))))))))

#define module_name(name)					__meta_write(meta_annotate_t, m_name, (name))
#define module_version(version)				__meta_write(meta_version_t, m_version, (version))
#define module_author(author)				__meta_write(meta_annotate_t, m_author, (author))
#define module_description(description)		__meta_write(meta_description_t, m_description, (description))
#define module_dependency(dependency)		__meta_write(meta_dependency_t, m_dependency, (dependency))

#define module_oninitialize(function)		__meta_cbwrite(meta_callback_bv_t, m_init, (#function), "b:v", "", (function))
#define module_ondestroy(function)			__meta_cbwrite(meta_callback_vv_t, m_destroy, (#function), "v:v", "", (function))
#define module_onpreactivate(function)		__meta_cbwrite(meta_callback_vv_t, m_preactivate, (#function), "v:v", "", (function))
#define module_onpostactivate(function)		__meta_cbwrite(meta_callback_vv_t, m_postactivate, (#function), "v:v", "", (function))

#define define_syscall(function, sig, desc)		__meta_cbwrite(meta_callback_t, m_syscall, (#function), (sig), (desc), (meta_callback_f)(function))

#define config_param(variable, sig, desc)		__meta_write(meta_variable_t, m_configparam, (#variable), (sig), (desc), (meta_variable_m)&(variable))

#define cal_param(variable, sig, desc)			__meta_write(meta_variable_t, m_calparam, (#variable), (sig), (desc), (meta_variable_m)&(variable))
#define cal_onmodechange(function)				__meta_cbwrite(meta_callback_vi_t, m_calmodechange, (#function), "v:i", "", (function))
#define cal_onpreview(function)					__meta_cbwrite(meta_callback_bscpp_t, m_calpreview, (#function), "b:scpp", "", (function))

#define define_block(block_name, block_desc, constructor, sig, constructor_desc) \
												__meta_write(meta_block_t, m_block, (#block_name), (block_desc), (#constructor), (sig), (constructor_desc), (meta_callback_f)(constructor))
#define block_onupdate(block_name, function)	__meta_cbwrite(meta_blockcallback_t, m_blockupdate, (#function), "v:p", "", (#block_name), (function))
#define block_ondestroy(block_name, function)	__meta_cbwrite(meta_blockcallback_t, m_blockdestroy, (#function), "v:p", "", (#block_name), (function))
#define block_input(block_name, input_name, sig, desc) \
												__meta_write(meta_blockio_t, m_blockinput, (#block_name), (#input_name), (sig), (desc))
#define block_output(block_name, output_name, sig, desc) \
												__meta_write(meta_blockio_t, m_blockoutput, (#block_name), (#output_name), (sig), (desc))


typedef struct
{
	char name[META_SIZE_ANNOTATE] __meta_a1;
	double version __meta_a1;
	char author[META_SIZE_ANNOTATE] __meta_a1;
	char description[META_SIZE_LONGDESCRIPTION] __meta_a1;
	char dependencies[META_MAX_DEPENDENCIES][META_SIZE_ANNOTATE] __meta_a1;
	metasize_t dependencies_length __meta_a1;

	char init_name[META_SIZE_FUNCTION] __meta_a1;
	char destroy_name[META_SIZE_FUNCTION] __meta_a1;
	char preact_name[META_SIZE_FUNCTION] __meta_a1;
	char postact_name[META_SIZE_FUNCTION] __meta_a1;
	meta_callback_bv_f init __meta_a1;
	meta_callback_vv_f destroy __meta_a1;
	meta_callback_vv_f preact __meta_a1;
	meta_callback_vv_f postact __meta_a1;

	struct
	{
		char syscall_name[META_SIZE_FUNCTION] __meta_a1;
		char syscall_signature[META_SIZE_SIGNATURE] __meta_a1;
		char syscall_description[META_SIZE_SHORTDESCRIPTION] __meta_a1;
		meta_callback_f function __meta_a1;
	} syscalls[META_MAX_SYSCALLS] __meta_a1;
	metasize_t syscalls_length __meta_a1;

	struct
	{
		char config_name[META_SIZE_VARIABLE] __meta_a1;
		char config_signature __meta_a1;
		char config_description[META_SIZE_SHORTDESCRIPTION] __meta_a1;
		meta_variable_m variable __meta_a1;
	} configparams[META_MAX_CONFIGPARAMS] __meta_a1;
	metasize_t configparams_length __meta_a1;

	struct
	{
		char cal_name[META_SIZE_VARIABLE] __meta_a1;
		char cal_signature __meta_a1;
		char cal_description[META_SIZE_SHORTDESCRIPTION] __meta_a1;
		meta_variable_m variable __meta_a1;
	} calparams[META_MAX_CALPARAMS] __meta_a1;
	metasize_t calparams_length __meta_a1;
	char cal_modechange_name[META_SIZE_FUNCTION] __meta_a1;
	char cal_preview_name[META_SIZE_FUNCTION] __meta_a1;
	meta_callback_vi_f cal_modechange __meta_a1;
	meta_callback_bscpp_f cal_preview __meta_a1;

	struct
	{
		char block_name[META_SIZE_BLOCKNAME] __meta_a1;
		char block_description[META_SIZE_LONGDESCRIPTION] __meta_a1;
		char constructor_name[META_SIZE_FUNCTION] __meta_a1;
		char constructor_signature[META_SIZE_SIGNATURE] __meta_a1;
		char constructor_description[META_SIZE_SHORTDESCRIPTION] __meta_a1;
		meta_callback_f constructor;

		char update_name[META_SIZE_FUNCTION] __meta_a1;
		char destroy_name[META_SIZE_FUNCTION] __meta_a1;
		meta_callback_vp_f update;
		meta_callback_vp_f destroy;

		struct
		{
			char io_name[META_SIZE_BLOCKIONAME] __meta_a1;
			metaiotype_t io_type __meta_a1;
			char io_signature __meta_a1;
			char io_description[META_SIZE_SHORTDESCRIPTION] __meta_a1;
		} ios[META_MAX_BLOCKIOS] __meta_a1;
		metasize_t ios_length __meta_a1;
	} blocks[META_MAX_BLOCKS] __meta_a1;
	metasize_t blocks_length __meta_a1;
} meta_t;

#if defined(USE_BFD)
bool meta_parseelf(meta_t * meta, const char * path, exception_t ** err);
#endif

#ifdef __cplusplus
}
#endif
#endif

