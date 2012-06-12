#ifndef __KERNEL_H
#define __KERNEL_H

#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdbool.h>

#include <aul/common.h>
#include <aul/exception.h>
#include <aul/constraint.h>
#include <aul/log.h>
#include <aul/list.h>

#include <maxmodel/meta.h>
#include <kernel-types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	VERSION			"1.1"
#define	PROVIDER		"maxkernel.com"
#define	PROVIDER_URL	"http://www.maxkernel.com/"

#if defined(ALPHA) || defined(BETA)
  #define LOG(level, format, ...) log_write(level, "Module", "[%s in %s at %d]: " format, __FUNCTION__, __FILE__, __LINE__, ## __VA_ARGS__)
#else
  #define LOG(level, format, ...) log_write(level, "Module", format, ## __VA_ARGS__)
#endif

#define LOG1(level, format, ...) \
	({ static bool __h = false; if (!__h) { LOG(level, format, ## __VA_ARGS__); __h = true; } })

#define LOG_FATAL		LEVEL_FATAL
#define LOG_ERR			LEVEL_ERROR
#define LOG_WARN		LEVEL_WARNING
#define LOG_INFO		LEVEL_INFO
#define LOG_DEBUG		LEVEL_DEBUG

typedef struct __kobject_t kobject_t;

typedef void (*destructor_f)(kobject_t * object);
typedef ssize_t (*info_f)(kobject_t * object, char * buffer, size_t length);
typedef bool (*handler_f)(void * userdata);

struct __kobject_t
{
	const char * class_name;
	char * object_name;
	unsigned int object_id;
	kobject_t * parent;

	info_f info;
	destructor_f destructor;
	list_t objects_list;
};

void * kobj_new(const char * class_name, const char * name, info_f info, destructor_f destructor, size_t size);
bool kobj_getinfo(const kobject_t * kobject, const char ** class_name, const char ** object_name, const kobject_t ** parent);
void kobj_makechild(kobject_t * parent, kobject_t * child);
void kobj_destroy(kobject_t * object);
#define kobj_cast(kobj)		(&(kobj)->kobject)
#define kobj_id(kobj)		((kobj)->object_id)

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
bool kthread_newinterval(const char * name, int priority, double rate_hz, handler_f threadfunc, void * userdata, exception_t ** err);
bool kthread_newthread(const char * name, int priority, handler_f threadfunc, handler_f stopfunc, void * userdata, exception_t ** err);

#define SYSCALL_BUFFERMAX		256		// TODO - move this constant to a private define in console.c
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

const void * rategroup_input(const char * name);
void rategroup_output(const char * name, const void * output);
#define input(name)					rategroup_input(#name)
#define output(name, value)			rategroup_output(#name, value)

const char * max_model();
const char * kernel_id();
int kernel_installed();
int64_t kernel_timestamp();
int64_t kernel_elapsed();
const char * kernel_datatype(char type);

const char * kernel_loghistory();
string_t kernel_logformat(level_t level, const char * domain, uint64_t milliseconds, const char * message);

typedef enum
{
	calmode_runtime	= 0,
	calmode_calibrating	= 1,
} calmode_t;

typedef enum
{
	calstatus_ok		= 0,
	calstatus_canceled	= 1,
	//calstatus_autocal	= 3		// TODO - provide auto calibration
} calstatus_t;

typedef void (*calpreview_f)(void * object, const char * domain, const char * name, const char sig, void * backing, char * hint, size_t hint_length);
typedef void (*calgpreview_f)(void * object, const char * domain, const char * name, const char sig, const void * backing, char * hint, size_t hint_length);
typedef void (*calmodechange_f)(void * object, calmode_t mode, calstatus_t status);

#define cal_register(domain, name, variable, sig, desc, onpreview, onpreview_object) \
	({ \
		exception_t * __e = NULL;			\
		cal_doregister((domain), (name), (sig), constraint_parse((desc), &__e), constraint_parsepast(desc), (variable), (onpreview), (onpreview_object)); \
		if (exception_check(&__e))			\
		{									\
			LOG(LOG_WARN, "Invalid constraint declaration: %s. Ignoring constraint", __e->message); \
			exception_free(__e);			\
		}									\
	})

void cal_doregister(const char * domain, const char * name, const char sig, const constraint_t constraints, const char * desc, void * backing, calpreview_f onpreview, void * onpreview_object);
void cal_onpreview(const char * domain, calgpreview_f callback, void * object);
void cal_onmodechange(calmodechange_f callback, void * object);

calmode_t cal_getmode();
void cal_start();
const char * cal_preview(const char * domain, const char * name, const char * value, char * hint, size_t hint_length);
void cal_commit(const char * comment);
void cal_revert();
iterator_t cal_iterator();
bool cal_next(iterator_t itr, const char ** domain, const char ** name, char * sig, const constraint_t ** constraints, const char ** desc, const char ** value);


#ifdef __cplusplus
}
#endif
#endif
