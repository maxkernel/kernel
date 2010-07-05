#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <argp.h>
#include <malloc.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <pthread.h>
#include <bfd.h>
#include <confuse.h>
#include <sqlite.h>

#include <aul/common.h>
#include <aul/contrib/list.h>
#include <aul/mainloop.h>
#include <aul/mutex.h>

#include "kernel.h"
#include "kernel-priv.h"

List * modules = NULL;
module_t * kernel_module = NULL;
GHashTable * properties = NULL;
GHashTable * kthreads = NULL;
List * calentries = NULL;
GHashTable * syscalls = NULL;

mutex_t * io_lock = NULL;
sqlite * database = NULL;
uint64_t starttime = 0;

static struct list_head objects = {0};
static mutex_t kobj_mutex;

static struct list_head kthread_tasks = {0};
static mutex_t kthread_tasks_mutex;

static char logbuf[LOGBUF_SIZE] = {0};
static size_t loglen = 0;

static char cache_syscalls[CACHESTR_SIZE] = {0};
static char cache_modules[CACHESTR_SIZE] = {0};

static struct {
	//char * groundstation;
} args = { };

/*------------ SIGNALS -------------*/
static void sig_int(int signo)
{
	mainloop_stop(NULL);
}

static void sig_coredump(const char * name, const char * code)
{
	signal(SIGABRT, SIG_DFL);

	char timebuf[50];
	time_t now = time(NULL);
	strftime(timebuf, sizeof(timebuf), "%F.%H.%M.%S", localtime(&now));

	String file = string_new("%s/%s.%s.log", LOGDIR, code, timebuf);
	String cmd = string_new("/bin/bash %s/debug/core.debug.bash %d %s '%s'", INSTALL, getpid(), file.string, name);

	//execute the script file the uses gdb to grab a stack trace
	system(cmd.string);

	//now output the log
	FILE * fp = fopen(file.string, "a");
	if (fp != NULL)
	{
		fprintf(fp, "\n\n\nLog : ================================================================== [%s]\n", timebuf);
		fwrite(logbuf, sizeof(char), loglen, fp);
		fflush(fp);
		fclose(fp);
	}

	LOGK(LOG_ERR, "Kernel has received signal %s", code);
	log_destroy();

	LOGK(LOG_FATAL, "%s.", name);
}

static void sig_segv(int signo)
{
	sig_coredump("Segmentation fault", "segv");
}

static void sig_abrt(int signo)
{
	sig_coredump("Abort", "abrt");
}

static void sig_init()
{
	signal(SIGINT, sig_int);
	signal(SIGSEGV, sig_segv);
	signal(SIGBUS, sig_segv);
	signal(SIGABRT, sig_abrt);
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
}

/*-----------  ARGS ---------------*/
static error_t parse_args(int key, char * arg, struct argp_state * state)
{
	switch (key)
	{
		/*
		case 'g':
		{
			args.groundstation = calloc(strlen(arg)+1, sizeof(char));
			strcpy(args.groundstation, arg);
			break;
		}
		*/
		
	}
	
	return 0;
}

static struct argp_option arg_opts[] = {
	//TODO - add more args
	//{ "ground-station", 'g', "IP", 0, "Use IP as ground station IP, don't attempt to autodetect", 0 },
	{ 0 }
};

static struct argp argp = { arg_opts, parse_args, 0, 0 };


/*---------------------- CONFUSE ----------------------*/
static int cfg_loadmodule(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
	if (argc < 1)
	{
		cfg_error(cfg, "Invalid argument length for loadmodule function");
		return -1;
	}

	setpath(cfg_getstr(cfg, "path"));

	int i=0;
	for (; i<argc; i++)
	{
		if (!module_load(argv[i]))
		{
			LOGK(LOG_FATAL, "module_load failed");
		}
	}
	
	return 0;
}

static int cfg_config(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
	if (argc != 3)
	{
		cfg_error(cfg, "Invalid argument length for config function");
		return -1;
	}

	cfg_setparam(argv[0], argv[1], argv[2]);

	return 0;
}

static int cfg_execlua(cfg_t *cfg, cfg_opt_t *opt, int argc, const char **argv)
{
	if (argc < 1)
	{
		cfg_error(cfg, "Invalid argument length for execlua function");
		return -1;
	}

	setpath(cfg_getstr(cfg, "path"));

	int i=0;
	for (; i<argc; i++)
	{
		if (!lua_execfile(argv[i]))
		{
			LOGK(LOG_FATAL, "lua_execfile failed");
		}
	}

	return 0;
}

static void cfg_errorfunc(cfg_t * cfg, const char * fmt, va_list ap)
{
	String str;
	string_clear(&str);
	string_vappend(&str, fmt, ap);

	LOGK(LOG_ERR, "Configuration error: %s", str.string);
}


/*------------- LOGGING ------------------*/
static void log_appendbuf(Level level, const char * domain, uint64_t milliseconds, const char * message, void * userdata)
{
	String msg = kernel_logformat(level, domain, milliseconds, message);

	if (loglen + msg.length + 1 >= LOGBUF_SIZE)
	{
		log_removelistener(log_appendbuf);
		LOGK(LOG_WARN, "Too many log messages in logbuf. Won't continue saving them");
	}
	else
	{
		memcpy(logbuf + loglen, msg.string, msg.length);
		loglen += msg.length;
		logbuf[loglen] = 0;
	}
}

const char * kernel_loghistory()
{
	return logbuf;
}

String kernel_logformat(Level level, const char * domain, uint64_t milliseconds, const char * message)
{
	char * levelstr = "Unkno";
	if	((level & LEVEL_FATAL) > 0)			levelstr = "FATAL";
	else if	((level & LEVEL_ERROR) > 0)		levelstr = "Error";
	else if ((level & LEVEL_WARNING) > 0)	levelstr = "Warn";
	else if ((level & LEVEL_INFO) > 0)		levelstr = "Info";
	else if ((level & LEVEL_DEBUG) > 0)		levelstr = "Debug";

	return string_new("%s (%-5s) %" PRIu64 " - %s\n", domain, levelstr, milliseconds, message);
}

/*---------------- KOBJECT FUNCTIONS --------------------*/
void * kobj_new(const char * class_name, const char * name, info_f info, destructor_f destructor, size_t size)
{
	if (size < sizeof(kobject_t))
	{
		LOGK(LOG_FATAL, "Size of new kobject_t is too small");
		//will exit
	}

	kobject_t * object = malloc0(size);
	object->class_name = class_name;
	object->obj_name = name;
	object->info = info;
	object->destructor = destructor;

	kobj_register(object);
	return object;
}

void kobj_register(kobject_t * object)
{
	mutex_lock(&kobj_mutex);
	list_add(&object->kobjdb, &objects);
	mutex_unlock(&kobj_mutex);
}

void kobj_destroy(kobject_t * object)
{
	if (object == NULL)
	{
		return;
	}

	mutex_lock(&kobj_mutex);

	LOGK(LOG_DEBUG, "Destroying %s: %s", object->class_name, object->obj_name);
	if (object->destructor != NULL)
	{
		object->destructor(object);
	}

	list_del(&object->kobjdb);

	FREES(object->obj_name);
	FREE(object);

	mutex_unlock(&kobj_mutex);
}

/*---------------- THREAD FUNCTIONS ---------------------*/
static char * kthread_info(void * kthread)
{
	char * str = "[PLACEHOLDER KTHREAD INFO]";
	return strdup(str);
}

static void kthread_destroy(void * kthread)
{
	kthread_t * kth = kthread;
	kth->stop = TRUE;

	if (kth->running && kth != kthread_self())
	{
		if (kth->runnable->stopfunc != NULL)
		{
			kth->runnable->stopfunc(kth->runnable);
		}

		pthread_join(kth->thread, NULL);
	}
}

static void * kthread_dothread(void * object)
{
	kthread_t * kth = object;
	g_hash_table_insert(kthreads, (void *)pthread_self(), kth);

	//set scheduling parameters
	{
		struct sched_param param;
		if (sched_getparam(0, &param) == 0)
		{
			int prio = param.sched_priority + kth->priority;
			int max = sched_get_priority_max(KTHREAD_SCHED);
			int min = sched_get_priority_min(KTHREAD_SCHED);

			param.sched_priority = CLAMP(prio, min, max);
			if (sched_setscheduler(0, KTHREAD_SCHED, &param) == -1)
			{
				LOGK(LOG_WARN, "Could not set scheduler parameters on thread %s: %s", kth->kobject.obj_name, strerror(errno));
			}
		}
		else
		{
			LOGK(LOG_WARN, "Could not get scheduler parameters on thread %s: %s", kth->kobject.obj_name, strerror(errno));
		}
	}

	kth->thread = pthread_self();
	kth->running = TRUE;
	while (!kth->stop)
	{
		while (!kth->trigger->func(kth->trigger) && !kth->stop)
		{
			//this function will block or return false until triggered
		}

		if (!kth->stop)
		{
#ifdef EN_PROFILE
			int tmres = 0;
			struct timespec rtbefore, rtafter, cpubefore, cpuafter;

			tmres |= clock_gettime(CLOCK_REALTIME, &rtbefore);
			tmres |= clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpubefore);
#endif

			kth->runnable->runfunc(kth->runnable);	//this function will do the thread execution
													//rinse and repeat

#ifdef EN_PROFILE
			tmres |= clock_gettime(CLOCK_REALTIME, &rtafter);
			tmres |= clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cpuafter);

			if (tmres == 0)
			{
				uint64_t rtdiff = (rtafter.tv_sec - rtbefore.tv_sec) * AUL_NANOS_PER_SEC + (rtafter.tv_nsec - rtbefore.tv_nsec);
				uint64_t cpudiff = (cpuafter.tv_sec - cpubefore.tv_sec) * AUL_NANOS_PER_SEC + (cpuafter.tv_nsec - cpubefore.tv_nsec);

				profile_addthreadrealtime(kth, rtdiff);
				profile_addthreadcputime(kth, cpudiff);
			}
#endif
		}

		//yield before the next round
		sched_yield();
	}

	kth->running = FALSE;
	return NULL;
}

static boolean kthread_start(kthread_t * kth)
{
	pthread_t pthread;
	int result = pthread_create(&pthread, NULL, kthread_dothread, kth);

	if (result == -1)
	{
		LOGK(LOG_ERR, "Could not create new kernel thread: %s", strerror(result));
		return FALSE;
	}

	return TRUE;
}

void kthread_schedule(kthread_t * thread)
{
	kthread_task_t * task = malloc0(sizeof(kthread_task_t));
	String str = string_new("Starting thread '%s'", thread->kobject.obj_name);

	task->desc = string_copy(&str);
	task->thread = thread;
	task->task_func = kthread_start;

	mutex_lock(&kthread_tasks_mutex);
	list_add_tail(&task->list, &kthread_tasks);
	mutex_unlock(&kthread_tasks_mutex);
}

kthread_t * kthread_new(const char * name, trigger_t * trigger, runnable_t * runnable, int priority)
{
	kthread_t * kth = kobj_new("Thread", name, kthread_info, kthread_destroy, sizeof(kthread_t));
	kth->priority = priority;
	kth->running = FALSE;
	kth->trigger = trigger;
	kth->runnable = runnable;

	return kth;
}

kthread_t * kthread_self()
{
	return g_hash_table_lookup(kthreads, (void *)pthread_self());
}

runnable_t * kthread_getrunnable(kthread_t * kth)
{
	if (kth == NULL)
	{
		return NULL;
	}

	return kth->runnable;
}

trigger_t * kthread_gettrigger(kthread_t * kth)
{
	if (kth == NULL)
	{
		return NULL;
	}

	return kth->trigger;
}

static boolean kthread_dotasks(void * userdata)
{
	mutex_lock(&kthread_tasks_mutex);

	struct list_head * pos, * q;
	list_for_each_safe(pos, q, &kthread_tasks)
	{
		kthread_task_t * task = list_entry(pos, kthread_task_t, list);

		LOGK(LOG_DEBUG, "Executing thread task: %s", task->desc);
		task->task_func(task->thread);

		list_del(pos);
		FREES(task->desc);
		FREE(task);
	}

	mutex_unlock(&kthread_tasks_mutex);
	return TRUE;
}

static void kthread_dosinglepass(void * userdata)
{
	runfunction_t * rfunc = userdata;
	rfunc->dorunfunc(rfunc->userdata);

	kthread_t * kth = kthread_self();
	kth->stop = TRUE;
	FREE(rfunc);
}

static void kthread_stopsinglepass(void * userdata)
{
	runfunction_t * rfunc = userdata;
	if (rfunc->dostopfunc != NULL)
	{
		rfunc->dostopfunc(rfunc->userdata);
	}
}

void kthread_newinterval(const char * name, int priority, double rate_hz, handler_f threadfunc, void * userdata)
{
	String str = string_new("%s interval thread", name);

	trigger_t * trigger = trigger_newclock(name, rate_hz);
	runnable_t * runnable = exec_newfunction(name, threadfunc, NULL, userdata);
	kthread_t * kth = kthread_new(string_copy(&str), trigger, runnable, priority);

	kthread_schedule(kth);
}

void kthread_newthread(const char * name, int priority, handler_f threadfunc, handler_f stopfunc, void * userdata)
{
	runfunction_t * rfunc = malloc0(sizeof(runfunction_t));
	rfunc->dorunfunc = threadfunc;
	rfunc->dostopfunc = stopfunc;
	rfunc->userdata = userdata;

	String str = string_new("%s thread", name);

	trigger_t * trigger = trigger_newtrue(name);
	runnable_t * runnable = exec_newfunction(name, kthread_dosinglepass, kthread_stopsinglepass, rfunc);
	kthread_t * kth = kthread_new(string_copy(&str), trigger, runnable, priority);

	kthread_schedule(kth);
}

boolean kthread_requeststop()
{
	kthread_t * kth = kthread_self();
	return kth == NULL? FALSE : kth->stop;
}

/*---------------- KERN FUNCTIONS -----------------------*/
const char * max_model() { return property_get("model"); }
const char * kernel_id() { return property_get("id"); }
const int kernel_installed() { return property_get_i("installed"); }

//microseconds since epoch
const int64_t kernel_timestamp()
{
	static struct timeval now;
	gettimeofday(&now, NULL);

	uint64_t ts = (uint64_t)now.tv_sec * 1000000LL;
	ts += now.tv_usec;

	return ts;
}

//microseconds since kernel startup
const int64_t kernel_elapsed()
{
	return kernel_timestamp() - starttime;
}

const char * kernel_datatype(char type)
{
	switch (type)
	{
		case T_VOID:			return "void";
		case T_BOOLEAN:			return "boolean";
		case T_INTEGER:			return "integer";
		case T_DOUBLE:			return "double";
		case T_CHAR:			return "character";
		case T_STRING:			return "string";
		case T_BUFFER:			return "buffer";
		case T_ARRAY_BOOLEAN:	return "boolean array";
		case T_ARRAY_INTEGER:	return "integer array";
		case T_ARRAY_DOUBLE:	return "double array";

		default:				return "unknown";
	}
}

const char * syscall_list()
{
	size_t i = 0;

	GHashTableIter itr;
	syscall_t * syscall;

	g_hash_table_iter_init(&itr, syscalls);
	while (g_hash_table_iter_next(&itr, NULL, (void **)&syscall))
	{
		if (!strprefix(syscall->name, "__"))
		{
			// a prefix of __ means a hidden syscall
			i += snprintf(cache_syscalls + i, CACHESTR_SIZE - i, "%s(%s)\n", syscall->name, syscall->sig);
		}
	}

	return cache_syscalls;
}

const char * module_list()
{
	size_t i = 0;

	List * next = modules;
	while (next != NULL)
	{
		module_t * mod = next->data;
		if (mod != NULL && !strprefix(mod->kobject.obj_name, "__"))
		{
			i += snprintf(cache_modules + i, CACHESTR_SIZE - i, "%s\n", mod->path);
		}

		next = next->next;
	}

	return cache_modules;
}

const char * info(char * syscall_name)
{
	syscall_t * sys = syscall_get(syscall_name);
	if (sys == NULL)
	{
		return "Error: Syscall doesn't exist";
	}

	return sys->desc;
}


#define KERN_DEFSYSCALL(syscall, sig, desc) kern_defsyscall((syscall_f)syscall, #syscall, sig, desc);
static void kern_defsyscall(syscall_f func, char * name, char * sig, char * desc)
{
	syscall_t * syscall = malloc0(sizeof(syscall_t));
	syscall->name = strdup(name);
	syscall->desc = strdup(desc);
	syscall->sig = strdup(sig);
	syscall->func = func;

	kernel_module->syscalls = list_append(kernel_module->syscalls, syscall);
	syscall_reg(syscall);
}

/*-------------------- MAIN -----------------------*/
int main(int argc, char * argv[])
{
	//make sure i'm root
	if (getuid() != 0)
	{
		LOGK(LOG_FATAL, "Must be root!");
	}

	//do not prevent any unmounting
	chdir("/");
	
	//set up signals
	sig_init();

	//init logging
	log_openfile(LOGDIR "/" LOGFILE, NULL);
	log_addlistener(log_appendbuf, NULL, NULL);
	LOGK(LOG_INFO, "Welcome to MaxKernel v" VERSION " " RELEASE " by " PROVIDER);

	//log time
	{
		char timebuf[50];
		time_t now = time(NULL);
		strftime(timebuf, sizeof(timebuf), "%F %H:%M:%S", localtime(&now));
		LOGK(LOG_INFO, "The time is now %s", timebuf);
	}

	//initialize global variables
	INIT_LIST_HEAD(&objects);
	INIT_LIST_HEAD(&kthread_tasks);
	mutex_init(&kobj_mutex, M_RECURSIVE);
	mutex_init(&kthread_tasks_mutex, M_RECURSIVE);
	properties = g_hash_table_new(g_str_hash, g_str_equal);
	kthreads = g_hash_table_new(g_int_hash, g_int_equal);
	syscalls = g_hash_table_new(g_str_hash, g_str_equal);
	io_lock = mutex_new(M_RECURSIVE);

	//set start time
	starttime = kernel_timestamp();

	//configure memory for realtime
	if (mlockall(MCL_CURRENT | MCL_FUTURE))
	{
		LOGK(LOG_WARN, "Could not lock memory to RAM: %s", strerror(errno));
	}
	mallopt(M_TRIM_THRESHOLD, -1);

	//seed random number gen
	srand(time(NULL));

	//add some basic properties
	property_set("install", INSTALL);
	property_set("logdir", LOGDIR);
	property_set("config", INSTALL "/" CONFIG);
	property_set("model", MODEL);

	//connect to sqlite database
	LOGK(LOG_DEBUG, "Opening database file");
	database = sqlite_open(INSTALL "/" DBNAME, 0, NULL);
	if (database == NULL)
	{
		LOGK(LOG_ERR, "Could not open database file %s", DBNAME);
	}
	
	//init mainloop
	mainloop_init();

	//initialize library functions
	bfd_init();

	//initialize profiler
#ifdef EN_PROFILE
	profile_init();
#endif

	//initialize kernel vars and subsystems
	module_kernelinit();	//a generic module that points to kernel space
	
	//register syscalls
	KERN_DEFSYSCALL(	info,				"s:s",		"Returns description of the given syscall (param 1)");
	KERN_DEFSYSCALL(	module_list,		"s:v",		"Returns a string concatenation of paths of all loaded modules");
	KERN_DEFSYSCALL(	module_exists,		"b:s",		"Returns true if module exists and is loaded by name (param 1)");
	KERN_DEFSYSCALL(	syscall_list,		"s:v",		"Returns a string concatenation of all registered syscalls");
	KERN_DEFSYSCALL(	syscall_exists,		"b:ss",		"Returns true if syscall exists by name (param 1) and signature (param 2). If signature is an empty string or null, only name is evaluated");
	KERN_DEFSYSCALL(	max_model,			"s:v",		"Returns the model name of the robot");
	KERN_DEFSYSCALL(	kernel_id,			"s:v",		"Returns the unique id of the kernel (non-volatile)");
	KERN_DEFSYSCALL(	kernel_installed,	"i:v",		"Returns a unix timestamp when maxkernel was installed");
	KERN_DEFSYSCALL(	math_eval,			"d:s",		"Evaluates the given expression and returns the result. Expression can contain constants and function. ex: cos(2 * pi)");
	KERN_DEFSYSCALL(	property_get,		"s:s",		"Returns the string representation of the defined property");
	KERN_DEFSYSCALL(	property_set,		"v:ss",		"Sets the property name (param 1) to the specified value (param 2)");
	KERN_DEFSYSCALL(	property_clear,		"v:s",		"Clears the property name (param 1) and the associated value from the database");
	KERN_DEFSYSCALL(	property_isset,		"b:s",		"Returns true if the property name (param 1) has been set");

	//parse args
	LOGK(LOG_DEBUG, "Parsing prog arguments");
	if (argc > 0)
	{
		argp_parse(&argp, argc, argv, ARGP_SILENT, 0, 0);
	}
	
	//parse configuration file
	cfg_opt_t opts[] = {
		CFG_STR(	"id",			"(none)",				CFGF_NONE	),
		CFG_STR(	"path",			INSTALL "/modules",		CFGF_NONE	),
		CFG_STR(	"installed",	"0",					CFGF_NONE	),
		CFG_FUNC(	"loadmodule",	cfg_loadmodule			),
		CFG_FUNC(	"config",		cfg_config				),
		CFG_FUNC(	"execlua",		cfg_execlua				),
		CFG_END()
	};
	
	cfg_t * cfg = cfg_init(opts, 0);
	cfg_set_error_function(cfg, &cfg_errorfunc);
	
	LOGK(LOG_DEBUG, "Parsing configuration script " INSTALL "/" CONFIG);
	int result = cfg_parse(cfg, INSTALL "/" CONFIG);
	if (result != CFG_SUCCESS)
	{
		switch (result)
		{
			case CFG_FILE_ERROR:
				LOGK(LOG_FATAL, "Could not open configuration file " INSTALL "/" CONFIG);
			
			case CFG_PARSE_ERROR:
				LOGK(LOG_FATAL, "Parse error in configuration file " INSTALL "/" CONFIG);
			
			default:
				LOGK(LOG_FATAL, "Unknown error while opening/parsing configuration file " INSTALL "/" CONFIG);
		}
	}
	
	//register global properties
	property_set("id", cfg_getstr(cfg, "id"));
	property_set("installed", cfg_getstr(cfg, "installed"));

	//now free configuration struct
	cfg_free(cfg);


	//mainloop = g_main_loop_new(NULL, FALSE);
	/*
	{
		//analyze inputs and outputs on all blocks and make sure everything is kosher
		GHashTableIter itr;
		module_t * module;

		g_hash_table_iter_init(&itr, modules);
		while (g_hash_table_iter_next(&itr, NULL, (void **)&module))
		{
			GSList * next = module->block_inst;
			while (next != NULL)
			{
				block_inst_t * blk_inst = next->data;
				io_newcomplete(blk_inst);

				next = next->next;
			}
		}
	}

	{
		//analyze block instances and execution groups (kthreads with block execs) and make sure everything is good

		GHashTableIter itr;
		GSList * next;
		module_t * module;

		//build list of all block instances
		GSList * allblockinst = NULL;
		g_hash_table_iter_init(&itr, modules);
		while (g_hash_table_iter_next(&itr, NULL, (gpointer *)&module))
		{
			next = module->block_inst;
			while (next != NULL)
			{
				allblockinst = g_slist_append(allblockinst, next->data);
				next = next->next;
			}
		}

		//now loop through all kthreads and delete each block_inst_t from the list
		next = tmp_kthreads;
		while (next != NULL)
		{
			kthread_t * kth = next->data;
			exec_f * exec = kth->exec;

			if (exec_isblockinst(exec))
			{
				exec_block_t * eblk = (exec_block_t *)exec;

				GSList * next2 = eblk->inst_list;
				if (next2 == NULL)
				{
					LOGK(LOG_WARN, "Execution group has no block instance elements!");
					next = next->next;
					continue;
				}

				while (next2 != NULL)
				{
					block_inst_t * inst = next2->data;

					GSList * list_input = g_slist_find(allblockinst, inst);
					if (list_input != NULL)
					{
						list_input->data = NULL;
					}
					else
					{
						LOGK(LOG_FATAL, "Block instance %s:%s has been added to more than one execution group!", inst->block->module->path, inst->block->name);
						//will exit
					}

					next2 = next2->next;
				}
			}

			next = next->next;
		}

		//now iterate over list and warn on each non-null value
		next = allblockinst;
		while (next != NULL)
		{
			block_inst_t * blk_inst = next->data;
			if (blk_inst != NULL)
			{
				LOGK(LOG_WARN, "Block instance %s.%s has not been added to any execution group", blk_inst->block->module->path, blk_inst->block->name);
			}

			next = next->next;
		}

		g_slist_free(allblockinst);
	}
	*/
	//call post functions
	{
		List * next = NULL;

		//sort the calentries list
		list_sort(calentries, cal_compare);
		list_sort(modules, module_compare);

		//call init function on all modules
		next = modules;
		while (next != NULL)
		{
			module_t * module = next->data;
			module_init(module);
			next = next->next;
		}

		//check for new kernel tasks every second
		mainloop_addtimer(NULL, "KThread task handler", AUL_NANOS_PER_SEC, kthread_dotasks, NULL);

#ifdef EN_PROFILE
		mainloop_addtimer(NULL, "Profiler", (1.0 / PROFILE_UPDATEHZ) * AUL_NANOS_PER_SEC, profile_track, NULL);
#endif

		//execute all tasks in queue
		//task_exec();
/*
		//start up all kthreads
		GSList * next = tmp_kthreads;
		while (next != NULL)
		{
			kthread_t * kth = next->data;

			GError * err = NULL;
			g_thread_create(kthread_dothread, kth, FALSE, &err);
			if (err != NULL)
			{
				LOGK(LOG_FATAL, "Could not create new kernel thread: %s", err->message);
				//will exit
			}

			next = next->next;
		}

		g_slist_free(tmp_kthreads);
		tmp_kthreads = NULL;
*/
	}

	//run max kernel main loop
	//g_main_loop_run(mainloop);
	mainloop_run(NULL);


	LOGK(LOG_DEBUG, "Destroying the kernel.");
	mutex_lock(&kobj_mutex);

	//GHashTableIter itr;
	//kthread_t * kth;

	/*
	g_hash_table_iter_init(&itr, kthreads);
	while (g_hash_table_iter_next(&itr, NULL, (void **)&kth))
	{
		kth->stop = TRUE;
		pthread_join(kth->thread, NULL);
	}
	*/

	struct list_head * pos, * q;
	list_for_each_safe(pos, q, &objects)
	{
		kobject_t * item = list_entry(pos, kobject_t, kobjdb);
		kobj_destroy(item);
	}

	mutex_unlock(&kobj_mutex);

	if (database != NULL)
	{
		LOGK(LOG_DEBUG, "Closing database file");
		sqlite_close(database);
	}

	LOGK(LOG_INFO, "MaxKernel Exit.");
	log_destroy();

	return 0;
}

