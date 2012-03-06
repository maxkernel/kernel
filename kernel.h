#ifndef __KERNEL_H
#define __KERNEL_H

#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdbool.h>

#include <aul/common.h>
#include <aul/exception.h>
#include <aul/log.h>
#include <aul/list.h>

#include <kernel-types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	VERSION			"0.1"
#define	PROVIDER		"Senseta"
#define	PROVIDER_URL	"http://www.senseta.com/"

#if defined(ALPHA) || defined(BETA)
  #define LOG(level, format, ...) log_write(level, "Module", "[%s in %s at %d]: " format, __FUNCTION__, __FILE__, __LINE__, ## __VA_ARGS__)
#else
  #define LOG(level, format, ...) log_write(level, "Module", format, ## __VA_ARGS__)
#endif
#define LOG1(level, format, ...)  do { static bool __h = false; if (!__h) { LOG(level, format, ## __VA_ARGS__); __h = true; } } while(0)

#define LOG_FATAL		LEVEL_FATAL
#define LOG_ERR			LEVEL_ERROR
#define LOG_WARN		LEVEL_WARNING
#define LOG_INFO		LEVEL_INFO
#define LOG_DEBUG		LEVEL_DEBUG

#define ___META_CAT(a,b) _ ## a ## b
#define __META_CAT(a,b) ___META_CAT(a,b)
#define __WRITE_META(name, ...) \
	static const char __META_CAT(__meta_,__LINE__) [] \
	__attribute__((section(".meta"),unused)) = \
	name "(" #__VA_ARGS__ ")"

#define M_VERSION		"VERSION"
#define M_AUTHOR		"AUTHOR"
#define M_DESC			"DESCRIPTION"
#define M_DEP			"DEPENDENCY"
#define M_PREACT		"MODULE.PREACT"
#define M_POSTACT		"MODULE.POSTACT"
#define M_INIT			"MODULE.INIT"
#define M_DESTROY		"MODULE.DESTROY"
#define M_SYSCALL		"SYSCALL"
#define M_CFGPARAM		"CONFIG.PARAM"
#define M_CALPARAM		"CALIBRATION.PARAM"
#define M_CALUPDATE		"CALIBRATION.UPDATE"
#define M_CALPREVIEW	"CALIBRATION.PREVIEW"
#define M_BLOCK			"BLOCK"
#define M_ONUPDATE		"BLOCK.ONUPDATE"
#define M_ONDESTROY		"BLOCK.ONDESTROY"
#define M_INPUT			"BLOCK.INPUT"
#define M_OUTPUT		"BLOCK.OUTPUT"

#define MOD_KERNEL									"__kernel"
#define MOD_INIT(func)								__WRITE_META(M_INIT, #func)
#define MOD_PREACT(func)							__WRITE_META(M_PREACT, #func)
#define MOD_POSTACT(func)							__WRITE_META(M_POSTACT, #func)
#define MOD_DESTROY(func)							__WRITE_META(M_DESTROY, #func)
#define MOD_VERSION(ver)							__WRITE_META(M_VERSION, ver)
#define MOD_AUTHOR(author)							__WRITE_META(M_AUTHOR, author)
#define MOD_DESCRIPTION(desc)						__WRITE_META(M_DESC, desc)
#define MOD_DEPENDENCY(...)							__WRITE_META(M_DEP, __VA_ARGS__)

#define DEF_SYSCALL(name, sig, ...)					__WRITE_META(M_SYSCALL, #name, sig, ## __VA_ARGS__)

#define DEF_BLOCK(blk_name, constructor, sig, ...)	__def_block(blk_name, constructor, sig, ## __VA_ARGS__)
#define BLK_ONUPDATE(blk_name, func)				__blk_onupdate(blk_name, func)
#define BLK_ONDESTROY(blk_name, func)				__blk_ondestroy(blk_name, func)
#define BLK_INPUT(blk_name, name, sig, ...)			__blk_input(blk_name, name, sig, ## __VA_ARGS__)
#define BLK_OUTPUT(blk_name, name, sig, ...)		__blk_output(blk_name, name, sig, ## __VA_ARGS__)

#define STATIC										__static
#define STATIC_STR									"__static"
#define __def_block(blk_name, constructor, sig, ...) __WRITE_META(M_BLOCK, #blk_name, #constructor, sig, __VA_ARGS__)
#define __blk_onupdate(blk_name, func)				__WRITE_META(M_ONUPDATE, #blk_name, #func)
#define __blk_ondestroy(blk_name, func)				__WRITE_META(M_ONDESTROY, #blk_name, #func)
#define __blk_input(blk_name, name, sig, ...)		__WRITE_META(M_INPUT, #blk_name, #name, sig, __VA_ARGS__)
#define __blk_output(blk_name, name, sig, ...)		__WRITE_META(M_OUTPUT, #blk_name, #name, sig, __VA_ARGS__)

#define CFG_PARAM(name, sig, ...)					__WRITE_META(M_CFGPARAM, #name, sig, ## __VA_ARGS__)
#define CAL_PARAM(name, sig, desc)					__WRITE_META(M_CALPARAM, #name, sig, desc)
#define CAL_UPDATE(func)							__WRITE_META(M_CALUPDATE, #func)
#define CAL_PREVIEW(func)							__WRITE_META(M_CALPREVIEW, #func)

// TODO - delete me
//#define PARAMS_MAX				64


typedef void (*destructor_f)(void * object);
typedef char * (*info_f)(void * object);
typedef void (*handler_f)(void * userdata);

typedef struct __kobject_t
{
	const char * class_name;
	const char * object_name;
	struct __kobject_t * parent;

	info_f info;
	destructor_f destructor;
	list_t objects_list;
} kobject_t;

void * kobj_new(const char * class_name, const char * name, info_f info, destructor_f destructor, size_t size);
void kobj_register(kobject_t * object);		// TODO - delete this function!
void kobj_makechild(kobject_t * parent, kobject_t * child);
void kobj_destroy(kobject_t * object);

#define PATH_BUFSIZE			2048
#define PATH_MAXPATHS			50
typedef enum
{
	P_FILE			= (1 << 0),
	P_DIRECTORY		= (1 << 1),
	P_MODULE		= (1 << 2),
} ptype_t;
bool path_set(const char * newpath, exception_t ** err);
bool path_append(const char * path, exception_t ** err);
const char * path_resolve(const char * name, ptype_t type);
string_t path_join(const char * prefix, const char * file);

#define KTH_PRIO_MIN			1
#define KTH_PRIO_LOW			5
#define KTH_PRIO_MEDIUM			40
#define KTH_PRIO_HIGH			80
#define KTH_PRIO_MAX			99
void kthread_newinterval(const char * name, int priority, double rate_hz, handler_f threadfunc, void * userdata);
void kthread_newthread(const char * name, int priority, handler_f threadfunc, handler_f stopfunc, void * userdata);
bool kthread_requeststop();

#define SYSCALL_BUFFERMAX		128		// TODO - move this constant to a private define in console.c
typedef union
{
	bool t_bool;
	int t_int;
	double t_double;
	char t_char;
	const char * t_sting;
//	const buffer_t t_buffer;	// TODO - should we support buffers and arrays in syscalls?
//	const array_t t_array;
} sysreturn_t;

#define SYSCALL(name, ret, ...) syscall_exec(name, NULL, ret, ## __VA_ARGS__)
bool syscall_exists(const char * name, const char * sig);
bool syscall_exec(const char * name, exception_t ** err, void * ret, ...);
bool vsyscall_exec(const char * name, exception_t ** err, void * ret, va_list args);
bool asyscall_exec(const char * name, exception_t ** err, void * ret, void ** args);

void property_set(const char * name, const char * value);
void property_clear(const char * name);
const char * property_get(const char * name);
bool property_isset(const char * name);

const void * io_input(const char * name);
void io_output(const char * name, const void * value);
#define INPUT(name)					io_input(#name)
#define OUTPUT(name, value)			io_output(#name, value)

const char * max_model();
const char * kernel_id();
const int kernel_installed();
const int64_t kernel_timestamp();
const int64_t kernel_elapsed();
const char * kernel_datatype(char type);

const char * kernel_loghistory();
string_t kernel_logformat(level_t level, const char * domain, uint64_t milliseconds, const char * message);

typedef void (*cal_itr_d)(void * userdata, char * module, char * name, char type, char * desc, double value, double min, double max);
typedef void (*cal_itr_i)(void * userdata, char * module, char * name, char type, char * desc, int value, int min, int max);
void cal_iterate(cal_itr_i, cal_itr_d, void * userdata);
void cal_setparam(const char * module, const char * name, const char * value);
void cal_merge(const char * comment);
void cal_revert();
double math_eval(char * expr);

int parse_int(const char * s, exception_t ** err);
double parse_double(const char * s, exception_t ** err);
bool parse_bool(const char * s, exception_t ** err);

#ifdef __cplusplus
}
#endif
#endif
