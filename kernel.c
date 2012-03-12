#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <argp.h>
#include <malloc.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <pthread.h>
#include <bfd.h>
#include <confuse.h>
#include <sqlite3.h>

#include <glib.h>

#include <aul/common.h>
#include <aul/list.h>
#include <aul/hashtable.h>
#include <aul/mainloop.h>
#include <aul/iterator.h>
#include <aul/mutex.h>

#include <compiler.h>
#include <buffer.h>
#include <kernel.h>
#include <kernel-priv.h>

list_t modules;
list_t calentries;
hashtable_t properties;
hashtable_t syscalls;
module_t * kernel_module = NULL;
GHashTable * kthreads = NULL;

mutex_t io_lock;
sqlite3 * database = NULL;
uint64_t starttime = 0;

static list_t objects = {0};
static mutex_t kobj_mutex;

static list_t kthread_tasks = {0};
static mutex_t kthread_tasks_mutex;

static char logbuf[LOGBUF_SIZE] = {0};
static size_t loglen = 0;

static struct {
	bool forkdaemon;
} args = { false };



static void daemonize()
{
	pid_t pid, sid;

    // Fork off the parent process
	pid = fork();
	if (pid < 0)
	{
		exit(1);
	}

	// If we got a good PID, then we can exit the parent process.
	if (pid > 0)
	{
		exit(0);
	}

	// At this point we are executing as the child process

	// Create a new SID for the child process
	sid = setsid();
	if (sid < 0)
	{
		exit(1);
	}

	// Redirect standard files to /dev/null
	close(0);
	open("/dev/null", O_RDONLY);
	close(1);
	open("/dev/null", O_WRONLY);
	close(2);
	open("/dev/null", O_WRONLY);
}

/*------------ SIGNALS -------------*/
static void signal_int(int signo)
{
	mainloop_stop(NULL);
}

static void signal_coredump(const char * name, const char * code)
{
	signal(SIGABRT, SIG_DFL);

	char timebuf[50];
	time_t now = time(NULL);
	strftime(timebuf, sizeof(timebuf), "%F.%H.%M.%S", localtime(&now));

	string_t file = string_new("%s/%s.%s.log", LOGDIR, code, timebuf);
	string_t cmd = string_new("/bin/bash %s/debug/core.debug.bash %d %s '%s'", INSTALL, getpid(), file.string, name);

	// Execute the script file the uses gdb to grab a stack trace
	system(cmd.string);

	// Now output the log
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

static void signal_segv(int signo)
{
	signal_coredump("Segmentation fault", "segv");
}

static void signal_abrt(int signo)
{
	signal_coredump("Abort", "abrt");
}

static void signal_init()
{
	signal(SIGINT, signal_int);
	signal(SIGSEGV, signal_segv);
	signal(SIGBUS, signal_segv);
	signal(SIGABRT, signal_abrt);
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
}

/*-----------  ARGS ---------------*/
static error_t parse_args(int key, char * arg, struct argp_state * state)
{
	switch (key)
	{
		case 'd': args.forkdaemon = true;		break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	
	return 0;
}

static struct argp_option arg_opts[] = {
	{ "daemon",     'd',    0,          0, "start program as daemon", 0 },
	{ 0 }
};

static struct argp argp = { arg_opts, parse_args, 0, 0 };


/*---------------------- CONFUSE ----------------------*/
static int cfg_setpath(cfg_t * cfg, cfg_opt_t * opt, int argc, const char ** argv)
{
	if (argc != 1)
	{
		cfg_error(cfg, "Invalid argument length for setpath function");
		return -1;
	}

	exception_t * e = NULL;
	if (!path_set(argv[0], &e))
	{
		cfg_error(cfg, "Could not set path: Code %d %s", e->code, e->message);
		exception_free(e);
		return -1;
	}

	return 0;
}

static int cfg_appendpath(cfg_t * cfg, cfg_opt_t * opt, int argc, const char ** argv)
{
	if (argc < 1)
	{
		cfg_error(cfg, "Invalid argument length for appendpath function");
		return -1;
	}

	exception_t * e = NULL;
	for (int i = 0; i < argc; i++)
	{
		if (!path_append(argv[i], &e))
		{
			cfg_error(cfg, "Could not append path: Code %d %s", e->code, e->message);
			exception_free(e);
			return -1;
		}
	}

	return 0;
}

static int cfg_loadmodule(cfg_t * cfg, cfg_opt_t * opt, int argc, const char ** argv)
{
	if (argc < 1)
	{
		cfg_error(cfg, "Invalid argument length for loadmodule function");
		return -1;
	}

	for (int i = 0; i < argc; i++)
	{
		if (!module_load(argv[i]))
		{
			cfg_error(cfg, "Could not load module: %s!", argv[i]);
			return -1;
		}
	}
	
	return 0;
}

static int cfg_config(cfg_t * cfg, cfg_opt_t * opt, int argc, const char ** argv)
{
	if (argc != 3)
	{
		cfg_error(cfg, "Invalid argument length for config function");
		return -1;
	}

	cfg_setparam(argv[0], argv[1], argv[2]);

	return 0;
}

static int cfg_execfile(cfg_t * cfg, cfg_opt_t * opt, int argc, const char ** argv)
{
	if (argc < 1)
	{
		cfg_error(cfg, "Invalid argument length for execfile function");
		return -1;
	}

	for (int i = 0; i < argc; i++)
	{
		const char * file = argv[i];
		const char * prefix = path_resolve(file, P_FILE);

		if (prefix == NULL)
		{
			cfg_error(cfg, "Could not find file in path: %s", file);
			return -1;
		}

		string_t path = path_join(prefix, file);

		bool (*execfunc)(const char * path) = NULL;
		if (strsuffix(path.string, ".lua"))
		{
			execfunc = lua_execfile;
		}
		else
		{
			LOGK(LOG_WARN, "Could not determine how interpret file %s", path.string);
			continue;
		}

		if (!execfunc(path.string))
		{
			cfg_error(cfg, "Error while executing file %s", path.string);
			return -1;
		}
	}

	return 0;
}

static int cfg_execdir(cfg_t * cfg, cfg_opt_t * opt, int argc, const char ** argv)
{
	if (argc < 1)
	{
		cfg_error(cfg, "Invalid argument length for execdirectory function");
		return -1;
	}

	for (int i = 0; i < argc; i++)
	{
		const char * dir = argv[i];
		const char * prefix = path_resolve(dir, P_DIRECTORY);

		if (prefix == NULL)
		{
			cfg_error(cfg, "Could not find directory in path: %s", dir);
			return -1;
		}

		string_t path = path_join(prefix, dir);

		struct dirent * entry;
		DIR * d = opendir(path.string);
		while ((entry = readdir(d)) != NULL)
		{
			if (entry->d_name[0] == '.')
			{
				// Do not execute hidden files (also avoids . and ..)
				continue;
			}

			string_t abspath = path_join(path.string, entry->d_name);
			const char * abspath_str = abspath.string;
			if (cfg_execfile(cfg, opt, 1, &abspath_str) < 0)
			{
				return -1;
			}
		}
		closedir(d);
	}

	return 0;
}

static void cfg_errorfunc(cfg_t * cfg, const char * fmt, va_list ap)
{
	string_t str = string_blank();
	string_vappend(&str, fmt, ap);

	LOGK(LOG_FATAL, "%s", str.string);
}


/*------------- LOGGING ------------------*/
static void log_appendbuf(level_t level, const char * domain, uint64_t milliseconds, const char * message, void * userdata)
{
	string_t msg = kernel_logformat(level, domain, milliseconds, message);

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

string_t kernel_logformat(level_t level, const char * domain, uint64_t milliseconds, const char * message)
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
	object->object_name = name;
	object->info = info;
	object->destructor = destructor;

	kobj_register(object);
	return object;
}

void kobj_makechild(kobject_t * parent, kobject_t * child)
{
	child->parent = parent;
}

// TODO - remove this function! Merge with kobject_new
void kobj_register(kobject_t * object)
{
	mutex_lock(&kobj_mutex);
	{
		list_add(&objects, &object->objects_list);
	}
	mutex_unlock(&kobj_mutex);
}

void kobj_destroy(kobject_t * object)
{
	if (object == NULL)
	{
		return;
	}

	mutex_lock(&kobj_mutex);
	{
		LOGK(LOG_DEBUG, "Destroying %s: %s", object->class_name, object->object_name);
		if (object->destructor != NULL)
		{
			object->destructor(object);
		}

		list_remove(&object->objects_list);

		FREES(object->object_name);
		FREE(object);
	}
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
	kth->stop = true;

	if (kth->running && kth != kthread_self())
	{
		if (kth->runnable->stopfunc != NULL)
		{
			kth->runnable->stopfunc(kth->runnable);
		}

		pthread_join(kth->thread, NULL);
	}

	kobj_destroy(&kth->trigger->kobject);
	kobj_destroy(&kth->runnable->kobject);
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
				LOGK(LOG_WARN, "Could not set scheduler parameters on thread %s: %s", kth->kobject.object_name, strerror(errno));
			}
		}
		else
		{
			LOGK(LOG_WARN, "Could not get scheduler parameters on thread %s: %s", kth->kobject.object_name, strerror(errno));
		}
	}

	kth->thread = pthread_self();
	kth->running = true;
	while (!kth->stop)
	{
		while (!kth->trigger->func(kth->trigger) && !kth->stop)
		{
			//this function will block or return false until triggered
		}

		if (!kth->stop)
		{
			kth->runnable->runfunc(kth->runnable);	//this function will do the thread execution
													//rinse and repeat

			//yield before the next round
			sched_yield();
		}
	}

	kth->running = false;
	return NULL;
}

static bool kthread_start(kthread_t * kth)
{
	pthread_t pthread;
	int result = pthread_create(&pthread, NULL, kthread_dothread, kth);

	if (result == -1)
	{
		LOGK(LOG_ERR, "Could not create new kernel thread: %s", strerror(result));
		return false;
	}

	return true;
}

void kthread_schedule(kthread_t * thread)
{
	kthread_task_t * task = malloc0(sizeof(kthread_task_t));
	string_t str = string_new("Starting thread '%s'", thread->kobject.object_name);

	task->desc = string_copy(&str);
	task->thread = thread;
	task->task_func = kthread_start;

	mutex_lock(&kthread_tasks_mutex);
	list_add(&kthread_tasks, &task->list);
	mutex_unlock(&kthread_tasks_mutex);
}

kthread_t * kthread_new(const char * name, trigger_t * trigger, runnable_t * runnable, int priority)
{
	kthread_t * kth = kobj_new("Thread", name, kthread_info, kthread_destroy, sizeof(kthread_t));
	kth->priority = priority;
	kth->running = false;
	kth->trigger = trigger;
	kth->runnable = runnable;

	kobj_makechild(&kth->kobject, &kth->trigger->kobject);
	kobj_makechild(&kth->kobject, &kth->runnable->kobject);

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

static bool kthread_dotasks(mainloop_t * loop, uint64_t nanoseconds, void * userdata)
{
	mutex_lock(&kthread_tasks_mutex);

	list_t * pos, * q;
	list_foreach_safe(pos, q, &kthread_tasks)
	{
		kthread_task_t * task = list_entry(pos, kthread_task_t, list);

		LOGK(LOG_DEBUG, "Executing thread task: %s", task->desc);
		task->task_func(task->thread);

		list_remove(pos);
		FREES(task->desc);
		FREE(task);
	}

	mutex_unlock(&kthread_tasks_mutex);
	return true;
}

static void kthread_dosinglepass(void * userdata)
{
	runfunction_t * rfunc = userdata;
	rfunc->dorunfunc(rfunc->userdata);

	kthread_t * kth = kthread_self();
	kth->stop = true;
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
	string_t str = string_new("%s interval thread", name);

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

	string_t str = string_new("%s thread", name);

	trigger_t * trigger = trigger_newtrue(name);
	runnable_t * runnable = exec_newfunction(name, kthread_dosinglepass, kthread_stopsinglepass, rfunc);
	kthread_t * kth = kthread_new(string_copy(&str), trigger, runnable, priority);

	kthread_schedule(kth);
}

bool kthread_requeststop()
{
	kthread_t * kth = kthread_self();
	return kth == NULL? false : kth->stop;
}

/*---------------- KERN FUNCTIONS -----------------------*/
const char * max_model() { return property_get("model"); }
const char * kernel_id() { return property_get("id"); }
const int kernel_installed()
{
	const char * installed = property_get("installed");
	return (installed != NULL)? parse_int(installed, NULL) : 0;
}

// Microseconds since epoch
const int64_t kernel_timestamp()
{
	static struct timeval now;
	gettimeofday(&now, NULL);

	uint64_t ts = (uint64_t)now.tv_sec * 1000000LL;
	ts += now.tv_usec;

	return ts;
}

// Microseconds since kernel startup
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

static int modules_itr()
{
	const void * mods_next(void ** data)
	{
		list_t * list = *data = ((list_t *)*data)->next;
		return (list == &modules)? NULL : list_entry(list, module_t, global_list)->path;
	}

	return iterator_new(mods_next, NULL, &modules);
}

static const char * modules_next(int itr)
{
	const char * r = iterator_next(itr);
	return (r == NULL)? "" : r;
}

static int syscalls_itr()
{
	const void * scs_next(void ** data)
	{
		list_t * list = *data = ((list_t *)*data)->next;
		return (list == hashtable_itr(&syscalls))? NULL : hashtable_itrentry(list, syscall_t, global_entry)->name;
	}

	return iterator_new(scs_next, NULL, hashtable_itr(&syscalls));
}

static const char * syscalls_next(int itr)
{
	const char * r = iterator_next(itr);
	return (r == NULL)? "" : r;
}

static const char * syscall_info(char * syscall_name)
{
	syscall_t * sys = syscall_get(syscall_name);
	if (sys == NULL)
	{
		return "Error: Syscall doesn't exist";
	}

	return sys->desc;
}

static const char * syscall_signature(char * syscall_name)
{
	syscall_t * sys = syscall_get(syscall_name);
	if (sys == NULL)
	{
		return "";
	}

	return sys->sig;
}

static int properties_itr()
{
	const void * ps_next(void ** data)
	{
		list_t * list = *data = ((list_t *)*data)->next;
		return (list == hashtable_itr(&properties))? NULL : hashtable_itrentry(list, property_t, entry)->name;
	}

	return iterator_new(ps_next, NULL, hashtable_itr(&properties));
}

static const char * properties_next(int itr)
{
	const char * r = iterator_next(itr);
	return (r == NULL)? "" : r;
}

static void itr_free(int itr)
{
	iterator_free(itr);
}


#define KERN_DEFSYSCALL(syscall, sig, desc) kern_defsyscall((syscall_f)syscall, #syscall, sig, desc);
static void kern_defsyscall(syscall_f func, char * name, char * sig, char * desc)
{
	syscall_t * syscall = malloc0(sizeof(syscall_t));
	syscall->name = strdup(name);
	syscall->desc = strdup(desc);
	syscall->sig = strdup(sig);
	syscall->func = func;

	list_add(&kernel_module->syscalls, &syscall->module_list);
	syscall_reg(syscall);
}

/*-------------------- MAIN -----------------------*/
int main(int argc, char * argv[])
{
	// Make sure i'm root
	if (getuid() != 0)
	{
		LOGK(LOG_FATAL, "Must be root!");
	}

	// Runtime check for hard-requirements on kernel
	{
		if(*(int32_t *)"\200\0\0\0\0\0\0\0" < 0)
		{
			LOGK(LOG_FATAL, "Failed little-endian runtime check!");
		}
	}

	// Set up the working directory to the install dir
	chdir(INSTALL);
	path_set(".", NULL);
	
	// Set up signals
	signal_init();

	argp_parse(&argp, argc, argv, ARGP_SILENT, 0, 0);
	if (args.forkdaemon)
	{
		// Fork this process as a daemon
		daemonize();
	}

	// Init logging
	log_openfile(LOGDIR "/" LOGFILE, NULL);
	log_addlistener(log_appendbuf, NULL, NULL);

	// Configure memory for realtime
	if (mlockall(MCL_CURRENT | MCL_FUTURE))
	{
		LOGK(LOG_WARN, "Could not lock memory to RAM: %s", strerror(errno));
	}
	mallopt(M_TRIM_THRESHOLD, -1);

	// Set up buffers
	buffer_init();

	// Initialize library functions
	bfd_init();

	// Initialize global variables
	LIST_INIT(&modules);
	LIST_INIT(&calentries);
	LIST_INIT(&objects);
	LIST_INIT(&kthread_tasks);
	HASHTABLE_INIT(&properties, hash_str, hash_streq);
	HASHTABLE_INIT(&syscalls, hash_str, hash_streq);
	mutex_init(&kobj_mutex, M_RECURSIVE);
	mutex_init(&kthread_tasks_mutex, M_RECURSIVE);
	kthreads = g_hash_table_new(g_int_hash, g_int_equal);
	mutex_init(&io_lock, M_RECURSIVE);

	// Set start time
	starttime = kernel_timestamp();

	// Seed random number gen
	srand(time(NULL));

	//add some basic properties
	property_set("install", INSTALL);
	property_set("logdir", LOGDIR);
	property_set("config", INSTALL "/" CONFIG);
	
	// Init memfs file system
	{
		exception_t * e = NULL;
		if (!memfs_init(&e))
		{
			LOGK(LOG_FATAL, "%s", e->message);
			// Will exit
		}
	}

	// Create a log file in the memfs filesystem and log to it
	{
		int fd = memfs_newfd("run.log", O_RDWR | O_CREAT | O_TRUNC);
		log_addfd(fd, NULL);
	}

	// Do some basic logging
	LOGK(LOG_INFO, "Welcome to MaxKernel v%s %s by %s", VERSION, RELEASE, PROVIDER);
	LOGK(LOG_DEBUG, "Kernel compiled on %s with compiler version %s", __TIMESTAMP__, __VERSION__);
	{
		// Log time
		char timebuf[50];
		time_t now = time(NULL);
		strftime(timebuf, sizeof(timebuf), "%F %H:%M:%S", localtime(&now));
		LOGK(LOG_INFO, "The time is now %s", timebuf);
	}

	// Connect to sqlite database
	LOGK(LOG_DEBUG, "Opening database file");
	if (sqlite3_open(INSTALL "/" DBNAME, &database) != SQLITE_OK)
	{
		LOGK(LOG_ERR, "Could not open database file %s", DBNAME);
	}

	// Make pid file
	{
		int pfd = open(PIDFILE, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);
		if (pfd == -1)
		{
			LOGK(LOG_WARN, "Could not create pid file %s: %s", PIDFILE, strerror(errno));
		}
		else
		{
			string_t data = string_new("%u\n", getpid());
			if (write(pfd, data.string, data.length) != data.length)
			{
				LOGK(LOG_WARN, "Could not write all data to pid file %s", PIDFILE);
			}
			close(pfd);
		}
	}

	// Init AUL components
	mainloop_init();
	iterator_init();

	// Initialize kernel vars and subsystems
	module_kernelinit();	//a generic module that points to kernel space
	
	// Register syscalls
	KERN_DEFSYSCALL(	modules_itr,		"i:v",		"Returns an iterator that loops over all loaded modules. Use in conjunction with 'modules_next' and 'itr_free'");
	KERN_DEFSYSCALL(	modules_next,		"s:i",		"Returns the next name of module in iterator. Use in conjunction with 'modules_itr' and 'itr_free'");
	KERN_DEFSYSCALL(	module_exists,		"b:s",		"Returns true if module exists and is loaded by name (param 1)");
	KERN_DEFSYSCALL(	syscalls_itr,		"i:v",		"Returns an iterator that loop over all registered syscalls. Use in conjunction with 'syscalls_next' and 'itr_free'");
	KERN_DEFSYSCALL(	syscalls_next,		"s:i",		"Returns the next name of the syscall in the iterator. Use in conjunction with 'syscalls_itr' and 'itr_free'");
	KERN_DEFSYSCALL(	syscall_info,		"s:s",		"Returns description of the given syscall (param 1)");
	KERN_DEFSYSCALL(	syscall_exists,		"b:ss",		"Returns true if syscall exists by name (param 1) and signature (param 2). If signature is an empty string or null, only name is evaluated");
	KERN_DEFSYSCALL(	syscall_signature,	"s:s",		"Returns the signature for the given syscall (param 1) if it exists, or an empty string if not");
	KERN_DEFSYSCALL(	max_model,			"s:v",		"Returns the model name of the robot");
	KERN_DEFSYSCALL(	kernel_id,			"s:v",		"Returns the unique id of the kernel (non-volatile)");
	KERN_DEFSYSCALL(	kernel_installed,	"i:v",		"Returns a unix timestamp when maxkernel was installed");
	KERN_DEFSYSCALL(	properties_itr,		"i:v",		"Returns an iterator that loops over all the property names defined. Use in conjunction with 'properties_next' and 'itr_free'");
	KERN_DEFSYSCALL(	properties_next,	"s:i",		"Returns the next property name in the iterator. Use in conjunction with 'properties_itr' and 'itr_free'");
	KERN_DEFSYSCALL(	property_get,		"s:s",		"Returns the string representation of the defined property");
	KERN_DEFSYSCALL(	property_set,		"v:ss",		"Sets the property name (param 1) to the specified value (param 2)");
	KERN_DEFSYSCALL(	property_clear,		"v:s",		"Clears the property name (param 1) and the associated value from the database");
	KERN_DEFSYSCALL(	property_isset,		"b:s",		"Returns true if the property name (param 1) has been set");
	KERN_DEFSYSCALL(    itr_free,			"v:i",		"Frees the given iterator. It can no longer be used after it's free'd");

	// Parse configuration file
	cfg_opt_t opts[] = {
		CFG_STR(	"id",			"(none)",				CFGF_NONE	),
		CFG_STR(	"path",			INSTALL "/modules",		CFGF_NONE	),
		CFG_STR(	"installed",	"0",					CFGF_NONE	),
		CFG_STR(	"model",		"(unknown)",			CFGF_NONE	),
		CFG_FUNC(	"setpath",		cfg_setpath				),
		CFG_FUNC(	"appendpath",	cfg_appendpath			),
		CFG_FUNC(	"loadmodule",	cfg_loadmodule			),
		CFG_FUNC(	"config",		cfg_config				),
		CFG_FUNC(	"execfile",		cfg_execfile			),
		CFG_FUNC(	"execdirectory",cfg_execdir				),
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
	property_set("model", cfg_getstr(cfg, "model"));

	//now free configuration struct
	cfg_free(cfg);


	//mainloop = g_main_loop_new(NULL, false);
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

	// Call post functions
	{
		list_t * pos;

		// Sort the calentries list
		list_sort(&calentries, cal_compare);
		list_sort(&modules, module_compare);

		// Call init function on all modules
		list_foreach(pos, &modules)
		{
			module_t * module = list_entry(pos, module_t, global_list);
			module_init(module);
		}

		// Check for new kernel thread tasks every second
		mainloop_addtimer(NULL, "KThread task handler", NANOS_PER_SECOND, kthread_dotasks, NULL);
	}

	// Run maxkernel mainloop
	mainloop_run(NULL);

	// Returning from the main loop means we've gotten a signal to exit
	LOGK(LOG_DEBUG, "Destroying the kernel.");
	mutex_lock(&kobj_mutex);
	{
		//GHashTableIter itr;
		//kthread_t * kth;

		/*
		g_hash_table_iter_init(&itr, kthreads);
		while (g_hash_table_iter_next(&itr, NULL, (void **)&kth))
		{
			kth->stop = true;
			pthread_join(kth->thread, NULL);
		}
		*/

		while (objects.next != &objects)
		{
			kobject_t * item = NULL;
			bool found = false;

			list_t * pos;
			list_foreach(pos, &objects)
			{
				item = list_entry(pos, kobject_t, objects_list);
				if (item->parent == NULL)
				{
					// We've found a root object
					found = true;
					break;
				}
			}

			if (!found)
			{
				LOGK(LOG_ERR, "Orphan kobject found: %s: %s", item->class_name, item->object_name);
			}

			LOG(LOG_DEBUG, "Destroying kobject tree %s: %s", item->class_name, item->object_name);
			kobj_destroy(item);
		}
	}
	mutex_unlock(&kobj_mutex);

	// Destroy the buffer subsystem
	buffer_destroy();

	// Destroy the memfs subsystem
	LOGK(LOG_DEBUG, "Unmounting kernel memfs");
	{
		exception_t * e = NULL;
		if (!memfs_destroy(&e))
		{
			LOGK(LOG_ERR, "%s", e->message);
			exception_free(e);
		}
	}

	// Clear all the properties
	{
		list_t * pos, * n;
		hashtable_foreach_safe(pos, n, &properties)
		{
			property_t * prop = hashtable_itrentry(pos, property_t, entry);
			property_clear(prop->name);
		}
	}

	// Close the database file
	if (database != NULL)
	{
		LOGK(LOG_DEBUG, "Closing database file");
		sqlite3_close(database);
	}

	// Remove pid file
	unlink(PIDFILE);

	LOGK(LOG_INFO, "MaxKernel Exit.");

	// Destroy the log subsystem
	log_destroy();

	// Goodbye
	return 0;
}
