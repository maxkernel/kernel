#ifndef __KERNEL_PRIV_H
#define __KERNEL_PRIV_H

#include <stdarg.h>
#include <inttypes.h>
#include <time.h>
#include <sched.h>
#include <ffi.h>

#include <aul/common.h>
#include <aul/log.h>
#include <aul/list.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(ALPHA) || defined(BETA)
  #define LOGK(level, format, ...) log_write(level, "KERNEL", "[%s in %s at %d]: " format, __FUNCTION__, __FILE__, __LINE__, ## __VA_ARGS__)
#else
  #define LOGK(level, format, ...) log_write(level, "KERNEL", format, ## __VA_ARGS__)
#endif

struct __block_t;
struct __block_inst_t;
struct __boutput_inst_t;
struct __binput_inst_t;
struct __kthread_t;

typedef void (*blind_f)();
typedef void * (*syscall_f)();
typedef void * (*variable_t);
typedef void (*calupdate_f)(const char * name, const char type, void * newvalue, void * target, const int justpreview);
typedef boolean (*trigger_f)(void * object);
typedef void * (*blk_constructor_f)();
typedef void (*blk_onupdate_f)(void * userdata);
typedef void (*blk_destroy_f)(void * userdata);
typedef void (*blk_link_f)(struct __boutput_inst_t * output, struct __binput_inst_t * input);
typedef boolean (*kthread_dotask_f)(struct __kthread_t * thread);


typedef struct
{
	kobject_t kobject;

	char * path;				//the path of the module, also the inst_id
	char * version;				//the version string
	char * author;				//the author string
	char * description;			//a description of the module
	void * module;				//the dlfcn.h dlopen'd module object
	blind_f preactivate;		//the function to call during preactivation phase
	blind_f postactivate;		//the function to call during postactivation phase
	blind_f initialize;			//the function to call during initialization phase
	blind_f destroy;			//the destroy function to call when exiting
	List * syscalls;			//a list of syscall_t syscalls
	List * cfgentries;			//a list of cfgentry_t config entries
	GHashTable * calentries;	//a table of calentry_t calibration entries (name -> calentry_t)
	calupdate_f calupdate;		//the function to call when the calibration has been updated
	GHashTable * blocks;		//a hash table of block_name -> block_t blocks defined in this module
	List * block_inst;			//a list of all block_inst_t block instances created
	struct __block_t * block_global;			//the global block_t block, or null if none
	struct __block_inst_t * block_global_inst;	//the global block id, or null if no global block
} module_t;

typedef struct
{
	char * version;
	char * author;
	char * description;
	List * dependencies;
	char * preactivate;
	char * postactivate;
	char * initialize;
	char * destroy;
	List * syscalls;
	List * cfgentries;
	List * calentries;
	char * calupdate;
	GHashTable * blocks;
} meta_t;

typedef struct
{
	kobject_t kobject;

	const char * name;			//a copy of pointer obj_name
	module_t * module;
	const char * desc;
	const char * sig;
	void * dynamic_data;		//if this is a dynamic syscall (inernal to the kernel), put any info here
	syscall_f func;
	ffi_cif * cif;
} syscall_t;

typedef struct
{
	kobject_t kobject;

	syscall_t * syscall;
	struct __block_t * block;
	struct __block_inst_t * block_inst;
} syscallblock_t;

typedef struct {
	module_t * module;
	gchar * name;
	gchar * desc;
	gchar type;
	variable_t variable;
} cfgentry_t;

typedef struct {
	module_t * module;
	char * name;
	char * desc;
	char * sig;
	void * active;		//active variable (module memory)
	void * preview;		//the previewed value (if we are currently calibrating, but haven't yet saved, this value is set)
	void * deflt;			//the default value read from module before update
} calentry_t;

typedef struct {
	gchar type;
	gchar value[25];
} calparam_t;

typedef struct
{
	kobject_t kobject;

	trigger_f func;
} trigger_t;

typedef struct
{
	kobject_t kobject;

	trigger_f func;

	struct timespec last_trigger;
	uint64_t interval_nsec;
} trigger_clock_t;

typedef struct
{
	kobject_t kobject;

	trigger_f func;

	struct timespec last_trigger;
	uint64_t interval_nsec;
	struct __block_inst_t * block_inst;
} trigger_varclock_t;

typedef struct __block_t
{
	kobject_t kobject;

	const char * name;
	module_t * module;
	const char * desc;
	const char * new_name;
	const char * new_sig;
	blk_constructor_f new;
	const char * onupdate_name;
	blk_onupdate_f onupdate;
	const char * ondestroy_name;
	blk_destroy_f ondestroy;
	List * inputs;
	List * outputs;
} block_t;

typedef struct {
	block_t * block;
	const char * name;
	char sig;
	const char * desc;
} bio_t;

typedef struct
{
	kobject_t kobject;

	handler_f runfunc;
	handler_f stopfunc;
} runnable_t;

typedef struct
{
	kobject_t kobject;

	handler_f runfunc;
	handler_f stopfunc;

	runnable_t ** list;
	size_t onitem;
} rungroup_t;

typedef struct
{
	kobject_t kobject;

	handler_f runfunc;
	handler_f stopfunc;

	handler_f dorunfunc;
	handler_f dostopfunc;
	void * userdata;
} runfunction_t;

typedef struct __block_inst_t
{
	kobject_t kobject;

	handler_f runfunc;
	handler_f stopfunc;

	block_t * block;
	blk_destroy_f ondestroy;
	gpointer userdata;
	GHashTable * inputs_inst;
	GHashTable * outputs_inst;
} block_inst_t;

typedef struct __boutput_inst_t
{
	block_inst_t * block_inst;
	bio_t * output;
	void * data;
	void * copybuf;
	boolean data_modified;
	boolean copybuf_modified;
	List * links;
	size_t numlinks;
} boutput_inst_t;

typedef struct __binput_inst_t
{
	block_inst_t * block_inst;
	bio_t * input;
	boutput_inst_t * src_inst;
	blk_link_f copy_func;
	void * data;
} binput_inst_t;

typedef struct
{
	gchar * updatefunc;
	gchar * update_freq_str;
	gdouble update_freq_hz;
} mclock_t;

typedef struct __kthread_t
{
	kobject_t kobject;

	pthread_t thread;
	int priority;
	volatile boolean running;
	volatile boolean stop;
	trigger_t * trigger;
	runnable_t * runnable;
} kthread_t;

typedef struct
{
	const char * desc;
	kthread_t * thread;
	kthread_dotask_f task_func;
	struct list_head list;
} kthread_task_t;

#define LOGFILE					"maxkernel.log"
#define LOGBUF_SIZE				(200 * 1024)		/* 200 KB */
#define CACHESTR_SIZE			(1024)				/* 1 KB */

#define PATH_MAX_SIZE			1024
#define PATH_MAX_ENTRIES		50
void setpath(const char * newpath);
void appendpath(const char * newentry);
const char * getpath();
const char * resolvepath(const char * name);
meta_t * meta_parse(const char * path);
module_t * module_get(const char * name);
gboolean module_exists(const char * name);
void module_init(const module_t * module);
int module_compare(const void * a, const void * b);
module_t * module_load(const char * name);
void module_kernelinit();

boolean lua_execfile(const char * name);

void syscall_reg(syscall_t * syscall);
syscall_t * syscall_get(const char * name);
void syscall_destroy(void * syscall);

syscallblock_t * syscallblock_new(const char * name, binput_inst_t ** params, const char * desc);
block_inst_t * syscallblock_getblockinst(syscallblock_t * sb);

#ifdef EN_PROFILE
#define PROFILE_UPDATEHZ		1.0
void profile_init();
void profile_addthreadrealtime(kthread_t * kth, uint64_t nanoseconds);
void profile_addthreadcputime(kthread_t * kth, uint64_t nanoseconds);
boolean profile_track(void * userdata);
#endif

#define KTHREAD_SCHED		SCHED_RR
kthread_t * kthread_new(const char * name, trigger_t * trigger, runnable_t * runnable, int priority);
void kthread_schedule(kthread_t * thread);
kthread_t * kthread_self();
runnable_t * kthread_getrunnable(kthread_t * thread);
trigger_t * kthread_gettrigger(kthread_t * thread);

void * trigger_new(const char * name, info_f info, destructor_f destructor, trigger_f trigfunc, size_t malloc_size);
trigger_t * trigger_newclock(const char * name, double freq_hz);
trigger_t * trigger_newvarclock(const char * name, double initial_freq_hz);
trigger_t * trigger_newtrue(const char * name);
block_inst_t * trigger_varclock_getblockinst(trigger_t * trigger);
binput_inst_t * trigger_varclock_getrateinput(trigger_t * trigger);

void * exec_new(const char * name, info_f info, destructor_f destructor, handler_f runfunc, handler_f stopfunc, size_t malloc_size);
runnable_t * exec_newrungroup(const char * name, runnable_t ** runnables);
runnable_t * exec_newfunction(const char * name, handler_f runfunc, handler_f stopfunc, void * userdata);
runnable_t * exec_getcurrent();

char * io_blockinfo(void * obj);
void io_blockfree(void * obj);
block_inst_t * io_newblock(block_t * blk, void ** args);
void io_beforeblock(block_inst_t * block);
void io_afterblock(block_inst_t * block);
const void * io_doinput(block_inst_t * blk, const char * name);
void io_dooutput(block_inst_t * blk, const char * name, const void * value, boolean docopy);

void io_newcomplete(block_inst_t * inst);
boolean io_route(boutput_inst_t * out, binput_inst_t * in);

cfgentry_t * cfg_getparam(const gchar * modname, const gchar * cfgname);
void cfg_setparam(const gchar * modname, const gchar * cfgname, const gchar * value);

calparam_t * cal_getparam(const gchar * name, const gchar * sig);
int cal_compare(const void * a, const void * b);
void cal_freeparam(calparam_t * val);

#ifdef __cplusplus
}
#endif
#endif
