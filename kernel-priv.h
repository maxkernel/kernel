#ifndef __KERNEL_PRIV_H
#define __KERNEL_PRIV_H

#include <stdarg.h>
#include <inttypes.h>
#include <time.h>
#include <sched.h>

#include <aul/common.h>
#include <aul/log.h>
#include <aul/list.h>
#include <aul/mainloop.h>

#include <maxmodel/meta.h>
#include <maxmodel/model.h>

#include <kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(ALPHA) || defined(BETA)
  #define LOGK(level, format, ...) log_write(level, "KERNEL", "[%s in %s at %d]: " format, __FUNCTION__, __FILE__, __LINE__, ## __VA_ARGS__)
#else
  #define LOGK(level, format, ...) log_write(level, "KERNEL", format, ## __VA_ARGS__)
#endif


#define CAL_SIZE_CACHE			AUL_STRING_MAXLEN
#define CONFIG_SIZE_CACHE		MODEL_SIZE_VALUE

typedef struct __block_t block_t;
typedef struct __blockinst_t blockinst_t;
//struct __boutput_inst_t;
//struct __binput_inst_t;
typedef struct __trigger_t trigger_t;
typedef struct __kthread_t kthread_t;
typedef struct __biobacking_t biobacking_t;
typedef struct __link_t link_t;

typedef void (*blind_f)();
typedef void (*closure_f)(void * ret, const void * args[], void * userdata);
typedef void * (*syscall_f)();
typedef void * (*variable_t);
//typedef void (*calibration_f)(const char * name, const char type, void * newvalue, void * target);
typedef bool (*trigger_f)(trigger_t * trigger, void * userdata);
//typedef void * (*blk_constructor_f)();
//typedef void (*blk_onupdate_f)(void * userdata);
//typedef void (*blk_destroy_f)(void * userdata);
//typedef void (*blk_link_f)(const void * output, bool output_isnull, size_t outsize, void * input, bool input_isnull, size_t insize);
typedef void (*link_f)(const link_t * link, const void * from, bool from_isnull, void * to, bool to_isnull);
//typedef bool (*kthread_dotask_f)(kthread_t * thread);
typedef bool (*runnable_f)(kthread_t * thread, void * userdata);
typedef void (*blockact_f)(void * userdata);


typedef struct
{
	char * name;
	char * value;
	hashentry_t entry;
} property_t;

typedef struct
{
	void * function;
	void * cif;
	void ** atypes;
	void * rtype;
} ffi_function_t;

typedef struct
{
	closure_f callback;
	void * closure;
	void * cif;
	void ** atypes;
	void * rtype;
	void * userdata;
} ffi_closure_t;

typedef struct
{
	kobject_t kobject;
	list_t global_list;

	const meta_t * backing;

	//char * path;				//the path of the module, also the inst_id
	//char * version;				//the version string
	//char * author;				//the author string
	//char * description;			//a description of the module
	//void * module;				//the dlfcn.h dlopen'd module object
	//blind_f preactivate;		//the function to call during preactivation phase
	//blind_f postactivate;		//the function to call during postactivation phase
	//blind_f initialize;			//the function to call during initialization phase
	//blind_f destroy;			//the destroy function to call when exiting
	list_t syscalls;			//a list of syscall_t syscalls
	//list_t dependencies;		//a list of dependency_t dependency objects
	list_t configs;				//a list of config_t config entries
	//list_t calentries;			//a list of calentry_t calibration entries (name -> calentry_t)
	//calibration_f calupdate;	//the function to call when the calibration has been updated
	//calibration_f calpreview;	//the function to call when a calibration item should be previewed
	list_t blocks;				//a list of all blocks defined in this module
	list_t blockinsts;			//a list of all block_inst_t block instances created
} module_t;

#if 0
typedef struct
{
	char * version;
	char * author;
	char * description;
	list_t dependencies;
	char * preactivate;
	char * postactivate;
	char * initialize;
	char * destroy;
	list_t syscalls;
	list_t cfgentries;
	list_t calentries;
	char * calupdate;
	char * calpreview;
	list_t blocks;
} meta_t;
#endif

#if 0
typedef struct
{
	char * modname;
	module_t * module;
	list_t module_list;
} dependency_t;
#endif

typedef struct
{
	kobject_t kobject;
	list_t module_list;
	hashentry_t global_entry;

	char * name;
	module_t * module;
	char * desc;
	char * sig;

	syscall_f func;
	ffi_function_t * ffi;
} syscall_t;

typedef struct
{
	kobject_t kobject;

	syscall_t * syscall;
	ffi_closure_t * closure;
	//block_t * block;
	blockinst_t * blockinst;

	biobacking_t * retbacking;

	size_t argcount;
	biobacking_t * argbacking[0];	// Must be last
} syscallblock_t;

typedef struct {
	kobject_t kobject;

	list_t module_list;
	const meta_variable_t * variable;
	char cache[CONFIG_SIZE_CACHE];
} config_t;

/*
typedef struct {
	module_t * module;
	list_t module_list;
	list_t global_list;
	char * name;
	char * desc;
	char * sig;
	void * active;		//active variable (module memory)
	void * preview;		//the previewed value (if we are currently calibrating, but haven't yet saved, this value is set)
	void * deflt;		//the default value read from module before update
} calentry_t;

typedef struct {
	char type;
	char value[25];
} calparam_t;
*/

typedef struct
{
	list_t calibration_list;
	char * domain;

	calgpreview_f callback;
	void * object;
} calpreview_t;

typedef struct
{
	list_t calibration_list;
	calmodechange_f callback;
	void * object;
} calmodechange_t;

typedef struct
{
	list_t calibration_list;
	list_t module_list;

	char * domain;
	char * name;
	char sig;
	constraint_t constraints;
	char * desc;

	char * checkpoint;
	char cache[CAL_SIZE_CACHE];
	void * backing;

	struct
	{
		calpreview_f callback;
		void * object;
	} onpreview;
} calentry_t;

typedef struct
{
	calmode_t mode;

	list_t entries;
	list_t previews;
	list_t modechanges;
} calibration_t;

struct __trigger_t
{
	kobject_t kobject;

	trigger_f function;
	void * userdata;
};

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
	blockinst_t * blockinst;
} trigger_varclock_t;

struct __block_t
{
	kobject_t kobject;
	list_t module_list;

	const meta_block_t * backing;
	blockact_f onupdate;
	blockact_f ondestroy;

	module_t * module;
	//const char * name;
	//const char * desc;
	//const char * new_name;
	//const char * new_sig;
	//blk_constructor_f new;
	//const char * onupdate_name;
	//blk_onupdate_f onupdate;
	//const char * ondestroy_name;
	//blk_destroy_f ondestroy;
	//list_t inputs;
	//list_t outputs;

	list_t insts;
};

/*
typedef struct {
	block_t * block;
	list_t block_list;

	const char * name;
	char sig;
	const char * desc;
} bio_t;
*/

/*
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
*/

struct __blockinst_t
{
	kobject_t kobject;
	list_t block_list;

	//model_blockinst_t * backing;
	const model_linkable_t * backing;
	block_t * block;

	//handler_f runfunc;
	//handler_f stopfunc;

	//list_t module_list;
	//block_t * block;
	//blk_destroy_f ondestroy;
	//void * userdata;
	list_t inputs;
	list_t outputs;

	void * userdata;
};

struct __biobacking_t
{
	size_t size;		// TODO - delete this field
	char sig;
	bool isnull;
	uint8_t data[0];
};

struct __link_t
{
	link_f function;
	union
	{
		size_t length;
	} data;
};

typedef struct
{
	blockinst_t * blockinst;
	list_t blockinst_list;

	model_linksymbol_t * symbol;
	link_t link;
	biobacking_t * backing;
} biolink_t;

typedef struct
{
	blockinst_t * blockinst;
	list_t rategroup_list;

	meta_iotype_t type;
	char name[MODEL_SIZE_NAME];
	biobacking_t * backing;			// TODO - rename iobacking or io
} riobacking_t;

typedef struct
{
	kobject_t kobject;
	list_t global_list;

	blockinst_t * active;
	list_t * activeriobackings;

	blockinst_t * group[MODEL_MAX_RATEGROUPELEMS + SENTINEL];
	list_t riobackings;
} rategroup_t;

/*
typedef struct __boutput_inst_t
{
	blockinst_t * blockinst;
	list_t blockinst_list;

	bio_t * output;

	void * stage1;
	void * stage2;
	bool stage1_isnull;
	bool stage2_isnull;

	list_t links;
} boutput_inst_t;

typedef struct __binput_inst_t
{
	blockinst_t * blockinst;
	list_t blockinst_list;

	bio_t * input;
	boutput_inst_t * src_inst;
	list_t boutput_inst_list;

	void * stage3;
	bool stage3_isnull;
} binput_inst_t;
*/

struct __kthread_t
{
	kobject_t kobject;
	list_t schedule_list;

	pthread_t thread;
	int priority;
	volatile bool running;
	volatile bool stop;
	trigger_t * trigger;

	runnable_f runfunction;

	// TODO - rename this destroyfunction (because it is called in all cased of threaddeath)
	runnable_f stopfunction;
	void * userdata;
};

/*
typedef struct
{
	char * desc;
	kthread_t * thread;
	kthread_dotask_f task_func;
	list_t list;
} kthread_task_t;
*/

#define LOGFILE					"maxkernel.log"
#define PIDFILE					"/var/run/maxkernel.pid"
#define LOGBUF_SIZE				(400 * 1024)		/* 400 KB */

typedef enum
{
	act_preact		= 1,
	act_postact		= 2,
} moduleact_t;
//meta_t * meta_parse(const char * path);
//module_t * module_get(const char * name);
module_t * module_lookup(const char * name);
const meta_t * module_getmeta(const module_t * module);
///const block_t * module_getblock(const module_t * module, const char * blockname);
//const blockinst_t * module_getstaticblockinst(const module_t * module);
bool module_exists(const char * name);
void module_init(const module_t * module);
void module_act(const module_t * module, moduleact_t act);
module_t * module_load(meta_t * meta);
//void module_kernelinit();

// Memfs functions
// TODO - clean these up and determine which ones to keep
bool memfs_init(exception_t ** err);
bool memfs_destroy(exception_t ** err);
int memfs_newfd(const char * name, int oflags);
void memfs_delete(const char * name);
int memfs_orphanfd();
void memfs_closefd(int fd);
int memfs_dupfd(int fd);
void memfs_syncfd(int fd);
off_t memfs_sizefd(int fd);
void memfs_setseekfd(int fd, off_t position);
off_t memfs_getseekfd(int fd);
void memfs_rewindfd(int fd);

syscall_t * syscall_new(const char * name, const char * sig, syscall_f func, const char * desc, exception_t ** err);
syscall_t * syscall_get(const char * name);
void syscall_destroy(void * syscall);

syscallblock_t * syscallblock_new(const model_linkable_t * syscall, const char * name, const char * sig, size_t numparams, const char * desc);
blockinst_t * syscallblock_getblockinst(syscallblock_t * sb);

#define KTHREAD_SCHED		SCHED_RR
kthread_t * kthread_new(const char * name, int priority, trigger_t * trigger, runnable_f runfunction, runnable_f stopfunction, void * userdata, exception_t ** err);
void kthread_schedule(kthread_t * thread);
kthread_t * kthread_self();
trigger_t * kthread_gettrigger(kthread_t * thread);

void * trigger_new(const char * name, info_f info, destructor_f destructor, trigger_f trigfunc, size_t malloc_size);
trigger_t * trigger_newclock(const char * name, double freq_hz);
trigger_t * trigger_newvarclock(const char * name, double initial_freq_hz);
//trigger_t * trigger_newtrue(const char * name);
blockinst_t * trigger_varclock_getblockinst(trigger_t * trigger);
///binput_inst_t * trigger_varclock_getrateinput(trigger_t * trigger);

//void * exec_new(const char * name, info_f info, destructor_f destructor, handler_f runfunc, handler_f stopfunc, size_t malloc_size);
//runnable_t * exec_newrungroup(const char * name, runnable_t ** runnables);
//runnable_t * exec_newfunction(const char * name, handler_f runfunc, handler_f stopfunc, void * userdata);
//runnable_t * exec_getcurrent();

ffi_function_t * function_build(void * function, const char * sig, exception_t ** err);
void function_free(ffi_function_t * ffi);
void function_call(ffi_function_t * ffi, void * ret, void ** args);
ffi_closure_t * closure_build(void * function, closure_f callback, const char * sig, void * userdata, exception_t ** err);
void closure_free(ffi_closure_t * ci);

block_t * block_new(module_t * module, const meta_block_t * backing, exception_t ** err);
void block_getonupdate(const block_t * block, blockact_f * onupdate);
void block_addinst(block_t * block, blockinst_t * blockinst);
iterator_t block_ioitr(const block_t * block);
bool block_ioitrnext(iterator_t itr, const meta_blockio_t ** blockio);

blockinst_t * blockinst_newempty(const char * name);
blockinst_t * blockinst_new(block_t * block, const model_linkable_t * backing, const char * name);
const block_t * blockinst_getblock(const blockinst_t * blockinst);
void blockinst_act(blockinst_t * blockinst, blockact_f callback);

biobacking_t * link_newbacking(char sig, exception_t ** err);
void link_freebacking(biobacking_t * backing);
static inline void link_copy(const link_t * link, const biobacking_t * from, biobacking_t * to)
{
	link->function(link, from, from->isnull, to->data, to->isnull);
	to->isnull = from->isnull;
}

rategroup_t * rategroup_new(const char * name);
bool rategroup_addblockinst(rategroup_t * rategroup, blockinst_t * blockinst, exception_t ** err);
void rategroup_schedule(rategroup_t * rategroup, trigger_t * trigger, int priority, exception_t ** err);

/*
char * io_blockinfo(void * obj);
void io_blockfree(void * obj);
blockinst_t * io_newblockinst(block_t * blk, void ** args);
void io_beforeblockinst(blockinst_t * block);
void io_afterblockinst(blockinst_t * block);
const void * io_doinput(blockinst_t * blk, const char * name);
void io_dooutput(blockinst_t * blk, const char * name, const void * value);
*/
///binput_inst_t * io_getbinput(const blockinst_t * blockinst, const char * name);
///boutput_inst_t * io_getboutput(const blockinst_t * blockinst, const char * name);

///void io_newcomplete(blockinst_t * inst);
///bool io_route(boutput_inst_t * out, binput_inst_t * in);

config_t * config_new(const meta_t * meta, const meta_variable_t * config_variable, exception_t ** err);
bool config_apply(config_t * config, const model_config_t * config_newvalue, exception_t ** err);

void cal_init();
void cal_sort();

#ifdef __cplusplus
}
#endif
#endif
