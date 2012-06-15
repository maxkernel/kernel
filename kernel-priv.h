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
typedef struct __trigger_t trigger_t;
typedef struct __kthread_t kthread_t;
typedef struct __iobacking_t iobacking_t;

typedef void (*blind_f)();
typedef void (*closure_f)(void * ret, const void * args[], void * userdata);
typedef void * (*syscall_f)();
typedef void * (*variable_t);
typedef bool (*trigger_f)(trigger_t * trigger);
typedef void (*link_f)(const void * linkdata, const void * from, bool from_isnull, void * to, bool to_isnull);
typedef bool (*runnable_f)(kthread_t * thread, kobject_t * object);
typedef meta_callback_vp_f blockact_f;
typedef meta_callback_f blockconstructor_f;
typedef meta_t * (*metalookup_f)(const char * filename);
typedef iobacking_t * (*portlookup_f)(const model_linksymbol_t * symbol);


typedef struct
{
	list_t inputs;
	list_t outputs;
} linklist_t;

typedef list_t portlist_t;

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

	meta_t * meta;
	list_t syscalls;			// The list of syscalls defined in the meta section of the module
	list_t configs;				// The list of config entries defined
	list_t blocks;				// The list of all blocks defined in this module
	list_t blockinsts;			// The list of all block instances created
} module_t;

typedef struct
{
	kobject_t kobject;
	list_t module_list;
	hashentry_t global_entry;

	char * name;
	module_t * module;
	char * desc;
	char * sig;

	syscall_f func;			// TODO - remove this member?
	ffi_function_t * ffi;
} syscall_t;

// TODO - rename this syscallblockinst_t maybe?
typedef struct
{
	kobject_t kobject;

	syscall_t * syscall;
	ffi_closure_t * closure;

	mutex_t lock;
	linklist_t links;
	portlist_t ports;
} syscallblock_t;

typedef struct {
	kobject_t kobject;

	list_t module_list;
	char * name;
	char sig;
	char * desc;

	const meta_variable_t * variable;	// TODO - remove this field!!!
	char cache[CONFIG_SIZE_CACHE];
} config_t;

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
};

typedef struct
{
	trigger_t trigger;
	struct timespec last_trigger;
	uint64_t interval_nsec;
	double freq_hz;
} trigger_clock_t;

typedef struct
{
	trigger_clock_t clock;
	linklist_t links;
	portlist_t ports;
} trigger_varclock_t;

struct __block_t
{
	kobject_t kobject;
	list_t module_list;

	module_t * module;
	char * name;
	char * desc;

	char * newsig;
	char * newdesc;
	ffi_function_t * new;

	blockact_f onupdate;
	blockact_f ondestroy;

	list_t insts;
};

struct __blockinst_t
{
	kobject_t kobject;
	list_t block_list;

	block_t * block;
	char * name;
	const char * const * args;		// TODO - make this a copy of the backing args (somehow!)
	size_t argslen;

	linklist_t links;
	void * userdata;
};

struct __iobacking_t
{
	char sig;
	bool isnull;
	uint8_t data[0];
};

typedef struct
{
	list_t link_list;

	const model_linksymbol_t * symbol;
	iobacking_t * backing;
	link_f linkfunction;
	void * linkdata;
} link_t;

typedef struct
{
	list_t port_list;

	meta_iotype_t type;
	char name[MODEL_SIZE_NAME];
	iobacking_t * backing;
} port_t;



typedef struct
{
	list_t rategroup_list;

	blockinst_t * blockinst;
	portlist_t ports;
} rategroup_blockinst_t;

typedef struct
{
	kobject_t kobject;
	list_t global_list;

	char * name;
	int priority;
	trigger_varclock_t * trigger;

	list_t blockinsts;
	rategroup_blockinst_t * active;
} rategroup_t;

struct __kthread_t
{
	kobject_t kobject;
	list_t schedule_list;

	pthread_t thread;
	int priority;
	volatile bool running;
	volatile bool stop;
	trigger_t * trigger;
	kobject_t * object;

	runnable_f runfunction;

	// TODO - rename this destroyfunction (because it is called in all cased of threaddeath)
	runnable_f stopfunction;
};

typedef struct
{
	kobject_t kobject;

	handler_f run;
	handler_f stop;
	void * userdata;
} kthreaddata_t;


#define LOGFILE					"maxkernel.log"
#define PIDFILE					"/var/run/maxkernel.pid"
#define LOGBUF_SIZE				(400 * 1024)		// 400 KB

#define SCHED_POLICY				SCHED_RR
#define SCHED_PRIO_BASE		5


typedef enum
{
	act_preact		= 1,
	act_postact		= 2,
} moduleact_t;
module_t * module_lookup(const char * name);
bool module_exists(const char * name);
void module_init(const module_t * module);
void module_activate(const module_t * module, moduleact_t act);
module_t * module_load(meta_t * meta, metalookup_f lookup, exception_t ** err);
block_t * module_lookupblock(module_t * module, const char * blockname);
#define module_meta(module)		((module)->meta)

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

// TODO rename syscallblock to something like syscallblockinst (because it's really an instance)
syscallblock_t * syscallblock_new(const model_linkable_t * linkable, exception_t ** err);
#define syscallblock_links(sb)	(&(sb)->links)
#define syscallblock_ports(sb)	(&(sb)->ports)

kthread_t * kthread_new(const char * name, int priority, trigger_t * trigger, kobject_t * object, runnable_f runfunction, runnable_f stopfunction, exception_t ** err);
void kthread_schedule(kthread_t * thread);
kthread_t * kthread_self();
#define kthread_trigger(kth)	((kth)->trigger)
#define kthread_object(kth)		((kth)->object)

void * trigger_new(const char * name, desc_f info, destructor_f destructor, trigger_f trigfunc, size_t malloc_size);
bool trigger_watch(trigger_t * trigger);
trigger_clock_t * trigger_newclock(const char * name, double freq_hz);
trigger_varclock_t * trigger_newvarclock(const char * name, double initial_freq_hz, exception_t ** err);
#define trigger_cast(t)			((trigger_t *)(t))
#define trigger_varclock_links(t)	(&(t)->links)
#define trigger_varclock_ports(t)	(&(t)->ports)

ffi_function_t * function_build(void * function, const char * sig, exception_t ** err);
void function_free(ffi_function_t * ffi);
void function_call(ffi_function_t * ffi, void * ret, void ** args);
ffi_closure_t * closure_build(void * function, closure_f callback, const char * sig, void * userdata, exception_t ** err);
void closure_free(ffi_closure_t * closure);

block_t * block_new(module_t * module, const meta_block_t * block, exception_t ** err);
void block_add(block_t * block, blockinst_t * blockinst);
void * block_callconstructor(block_t * block, void ** args);
bool block_iolookup(const block_t * block, const char * ioname, meta_iotype_t iotype, char * io_sig, const char ** io_desc);
iterator_t block_ioitr(const block_t * block);
bool block_ionext(iterator_t itr, const meta_blockio_t ** blockio);		// TODO - return the data out of blockio instead of a blockio
#define block_module(block)		((block)->module)
#define block_name(block)		((block)->name)
#define block_cbupdate(block)	((block)->onupdate)
#define block_cbdestroy(block)	((block)->ondestroy)
#define block_newsig(block)		((block)->newsig)

#define BLOCKINST_BUFFERMAX		256
blockinst_t * blockinst_new(const model_linkable_t * linkable, exception_t ** err);
bool blockinst_create(blockinst_t * blockinst, exception_t ** err);
void blockinst_act(blockinst_t * blockinst, blockact_f callback);
#define blockinst_block(blockinst)		((blockinst)->block)
#define blockinst_links(blockinst)		(&(blockinst)->links)
#define blockinst_userdata(blockinst)	((blockinst)->userdata)

iobacking_t * iobacking_new(char sig, exception_t ** err);
void iobacking_destroy(iobacking_t * backing);
void iobacking_copy(iobacking_t * backing, const void * data);
#define iobacking_sig(backing)	((backing)->sig)
#define iobacking_isnull(backing)	((backing)->isnull)
#define iobacking_data(backing)	((void *)(backing)->data)

#define linklist_init(l)		({ list_init(&(l)->inputs); list_init(&(l)->outputs); })
iobacking_t * link_connect(const model_link_t * link, char outsig, linklist_t * outlinks, char insig, linklist_t * inlinks, exception_t ** err);
void link_destroy(linklist_t * links);
link_f link_getfunction(const model_linksymbol_t * model_link, char from_sig, char to_sig, void ** linkdata);
void link_doinputs(portlist_t * ports, linklist_t * links);
void link_dooutputs(portlist_t * ports, linklist_t * links);
void link_sort(linklist_t * links);
#define link_symbol(link)		((link)->symbol)

#define portlist_init(l)		({ list_init(l); })
#define port_test(port, iotype, ioname)		((port)->type == (iotype) && strcmp((port)->name, (ioname)) == 0)
bool port_add(portlist_t * ports, meta_iotype_t type, const char * name, iobacking_t * backing, exception_t ** err);
void port_destroy(portlist_t * ports);
port_t * port_lookup(portlist_t * ports, meta_iotype_t type, const char * name);
bool port_makeblockports(const block_t * block, portlist_t * list, exception_t ** err);
#define port_iobacking(port)	((port)->backing)

rategroup_t * rategroup_new(const model_linkable_t * linkable, exception_t ** err);
bool rategroup_addblockinst(rategroup_t * rategroup, blockinst_t * blockinst, exception_t ** err);
bool rategroup_schedule(rategroup_t * rategroup, exception_t ** err);
#define rategroup_name(rg)		((rg)->name)
#define rategroup_links(rg)		(trigger_varclock_links((rg)->trigger))
#define rategroup_ports(rg)		(trigger_varclock_ports((rg)->trigger))

// TODO IMPORTANT - fix config_t to remove references to meta_variable_t!!
config_t * config_new(const meta_t * meta, const meta_variable_t * config, exception_t ** err);
bool config_apply(config_t * config, const model_config_t * newvalue, exception_t ** err);

void cal_init();
void cal_sort();

#ifdef __cplusplus
}
#endif
#endif
