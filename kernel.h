#ifndef __KERNEL_H
#define __KERNEL_H

#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <glib.h>
#include <gmodule.h>

#include <aul/common.h>
#include <aul/log.h>
#include <aul/contrib/list.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	VERSION			"0.1"
#define	PROVIDER		"Senseta"
#define	PROVIDER_URL	"http://www.senseta.com/"

#define T_METHOD		'M'
#define T_ERROR			'E'
#define T_RETURN		'R'
#define T_VOID			'v'
#define T_BOOLEAN		'b'
#define T_INTEGER		'i'
#define T_DOUBLE		'd'
#define T_CHAR			'c'
#define T_STRING		's'
#define T_ARRAY_BOOLEAN	'B'
#define T_ARRAY_INTEGER	'I'
#define T_ARRAY_DOUBLE	'D'
#define T_BUFFER		'x'

#define S_VOID			"v"
#define S_BOOLEAN		"b"
#define S_INTEGER		"i"
#define S_DOUBLE		"d"
#define S_CHAR			"c"
#define S_STRING		"s"
#define S_ARRAY_BOOLEAN	"B"
#define S_ARRAY_INTEGER	"I"
#define S_ARRAY_DOUBLE	"D"
#define S_BUFFER		"x"

#if defined(ALPHA) || defined(BETA)
  #define LOG(level, format, ...) log_write(level, "Module", "[%s in %s at %d]: " format, __FUNCTION__, __FILE__, __LINE__, ## __VA_ARGS__)
#else
  #define LOG(level, format, ...) log_write(level, "Module", format, ## __VA_ARGS__)
#endif

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
#define M_BLOCK			"BLOCK"
#define M_ONUPDATE		"BLOCK.ONUPDATE"
#define M_ONDESTROY		"BLOCK.ONDESTROY"
#define M_INPUT			"BLOCK.INPUT"
#define M_OUTPUT		"BLOCK.OUTPUT"

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

typedef void (*destructor_f)(void * object);
typedef char * (*info_f)(void * object);
typedef void (*handler_f)(void * userdata);

typedef struct
{
	const char * class_name;
	const char * obj_name;
	info_f info;
	destructor_f destructor;
	struct list_head kobjdb;
} kobject_t;

void * kobj_new(const char * class_name, const char * name, info_f info, destructor_f destructor, size_t size);
void kobj_register(kobject_t * object);
void kobj_destroy(kobject_t * object);

#define KTH_PRIO_MIN			1
#define KTH_PRIO_LOW			5
#define KTH_PRIO_MEDIUM			40
#define KTH_PRIO_HIGH			80
#define KTH_PRIO_MAX			99
void kthread_newinterval(const char * name, int priority, double rate_hz, handler_f threadfunc, void * userdata);
void kthread_newthread(const char * name, int priority, handler_f threadfunc, handler_f stopfunc, void * userdata);
boolean kthread_requeststop();

#define SYSCALL(name, ...) syscall_exec(name, ## __VA_ARGS__)
boolean syscall_exists(const char * name, const char * sig);
void * syscall_exec(const char * name, ...);
void * vsyscall_exec(const char * name, va_list args);
void * asyscall_exec(const char * name, void ** args);
void syscall_free(void * p);

void property_set(char * name, char * value);
void property_clear(const char * name);
char * property_get(const char * name);
int property_get_i(const char * name);
double property_get_d(const char * name);
boolean property_isset(const char * name);

const void * io_input(const char * name);
void io_output(const char * name, const void * value, boolean docopy);

#define ISNULL(name)				(INPUT(name) == NULL)
#define INPUT(name)					io_input(#name)
#define INPUTT(type, name)			(*(const type *)INPUT(name))
#define OUTPUT(name, value)			io_output(#name, value, TRUE)
#define OUTPUT_NOCOPY(name, value)	io_output(#name, value, FALSE)

const char * max_model();
const char * kernel_id();
const int kernel_installed();
const int64_t kernel_timestamp();
const int64_t kernel_elapsed();
const char * kernel_datatype(char type);

const char * kernel_loghistory();
String kernel_logformat(Level level, const char * domain, uint64_t milliseconds, const char * message);

typedef void (*cal_itr_d)(void * userdata, char * module, char * name, char type, char * desc, double value, double min, double max);
typedef void (*cal_itr_i)(void * userdata, char * module, char * name, char type, char * desc, int value, int min, int max);
void cal_iterate(cal_itr_i, cal_itr_d, void * userdata);
void cal_setparam(const char * module, const char * name, const char * value);
void cal_merge(const char * comment);
void cal_revert();
//double cal_map(const double dest_min, const double dest_center, const double dest_max, const double src_min, const double src_center, const double src_max, double value);

double math_eval(char * expr);

#ifdef __cplusplus
}
#endif
#endif
