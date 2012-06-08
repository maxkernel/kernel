#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <argp.h>
#include <malloc.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <pthread.h>
#include <confuse.h>
#include <sqlite3.h>

#include <aul/common.h>
#include <aul/list.h>
#include <aul/hashtable.h>
#include <aul/mainloop.h>
#include <aul/parse.h>
#include <aul/iterator.h>
#include <aul/mutex.h>

#include <maxmodel/model.h>
#include <maxmodel/meta.h>
#include <maxmodel/interpret.h>

#include <compiler.h>
#include <buffer.h>
#include <method.h>
#include <kernel.h>
#include <kernel-priv.h>

model_t * model = NULL;

list_t modules;
list_t rategroups;
//list_t calentries;
hashtable_t properties;
hashtable_t syscalls;
list_t kthreads;

mutex_t io_lock;
sqlite3 * database = NULL;
calibration_t calibration;
uint64_t starttime = 0;
threadlocal kthread_t * kthread_local = NULL;

static list_t kobjects = {0,0};
static mutex_t kobj_mutex;

static mutex_t kthreads_mutex;

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

// --------------------- Signal functions -----------------------
static void signal_int(int signo)
{
	unused(signo);

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
	unused(signo);

	signal_coredump("Segmentation fault", "segv");
}

static void signal_abrt(int signo)
{
	unused(signo);

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

// --------------------- Arg functions -----------------------
static error_t parse_args(int key, char * arg, struct argp_state * state)
{
	unused(arg);
	unused(state);

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
	{ 0,0,0,0,0,0 }
};

static struct argp argp = { arg_opts, parse_args, 0,0,0,0,0 };



// --------------------- Log functions -----------------------
static void log_appendbuf(level_t level, const char * domain, uint64_t milliseconds, const char * message, void * userdata)
{
	unused(userdata);

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

// --------------------- KObject functions -----------------------
void * kobj_new(const char * class_name, const char * name, info_f info, destructor_f destructor, size_t size)
{
	// Sanity check
	{
		if (size < sizeof(kobject_t))
		{
			LOGK(LOG_FATAL, "Size of new kobject_t is too small");
			//will exit
		}

		if (class_name == NULL || name == NULL)
		{
			LOGK(LOG_FATAL, "Invalid arguments to kobj_new");
			// Will exit
		}
	}

	kobject_t * object = malloc(size);
	memset(object, 0, size);

	object->class_name = class_name;
	object->object_name = strdup(name);
	object->info = info;
	object->destructor = destructor;

	mutex_lock(&kobj_mutex);
	{
		list_add(&kobjects, &object->objects_list);
	}
	mutex_unlock(&kobj_mutex);

	return object;
}

bool kobj_getinfo(const kobject_t * kobject, const char ** class_name, const char ** object_name, const kobject_t ** parent)
{
	// Sanity check
	{
		if (kobject == NULL)
		{
			return false;
		}
	}

	// TODO - return more kobject info (like callback functions)
	if (class_name != NULL)		*class_name = kobject->class_name;
	if (object_name != NULL)	*object_name = kobject->object_name;
	if (parent != NULL)			*parent = kobject->parent;

	return true;
}

void kobj_makechild(kobject_t * parent, kobject_t * child)
{
	// Sanity check
	{
		if unlikely(child == NULL)
		{
			return;
		}
	}

	child->parent = parent;
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

		free(object->object_name);
		free(object);
	}
	mutex_unlock(&kobj_mutex);
}

// --------------------- KThread functions -----------------------
static char * kthread_info(void * kthread)
{
	unused(kthread);

	char * str = "[PLACEHOLDER KTHREAD INFO]";
	return strdup(str);
}

static void kthread_destroy(void * kthread)
{
	kthread_t * kth = kthread;
	kth->stop = true;

	if (kth->stopfunction != NULL && !kth->stopfunction(kth, kth->object))
	{
		LOGK(LOG_WARN, "KThread stop function returned failure");
	}

	if (kth->running && kth != kthread_self())
	{
		// TODO - read thread return (2nd parameter)
		pthread_join(kth->thread, NULL);
	}

	kobj_destroy(kth->object);
	kobj_destroy(kobj_cast(kth->trigger));
}

static void * kthread_dothread(void * object)
{
	kthread_t * kth = object;
	kthread_local = kth;

	// Set scheduling parameters
	{
		struct sched_param param;
		memset(&param, 0, sizeof(struct sched_param));

		if (sched_getparam(0, &param) == 0)
		{
			int prio = param.sched_priority + kth->priority;
			int max = sched_get_priority_max(KTHREAD_SCHED);
			int min = sched_get_priority_min(KTHREAD_SCHED);

			param.sched_priority = clamp(prio, min, max);
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

	kth->running = true;
	kth->stop = false;
	while (!kth->stop)
	{
		while (!trigger_watch(kth->trigger) && !kth->stop)
		{
			// This function will block or return false until triggered
		}

		if (!kth->stop)
		{
			if (!kth->runfunction(kth, kth->object))		// This function will do the thread execution
			{
				break;
			}

			// Yield before the next round
			sched_yield();
		}
	}

	kth->running = false;
	return NULL;
}

static bool kthread_start(kthread_t * kth)
{
	int result = pthread_create(&kth->thread, NULL, kthread_dothread, kth);
	if (result == -1)
	{
		LOGK(LOG_ERR, "Could not create new kernel thread: %s", strerror(result));
		return false;
	}

	return true;
}

void kthread_schedule(kthread_t * thread)
{
	mutex_lock(&kthreads_mutex);
	{
		list_add(&kthreads, &thread->schedule_list);
	}
	mutex_unlock(&kthreads_mutex);
}

kthread_t * kthread_new(const char * name, int priority, trigger_t * trigger, kobject_t * object, runnable_f runfunction, runnable_f stopfunction, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}

		if (name == NULL || runfunction == NULL)
		{
			exception_set(err, EINVAL, "Invalid arguments!");
			return NULL;
		}
	}

	kthread_t * kth = kobj_new("Thread", name, kthread_info, kthread_destroy, sizeof(kthread_t));
	kth->priority = priority;
	kth->running = false;
	kth->stop = false;
	kth->trigger = trigger;
	kth->runfunction = runfunction;
	kth->stopfunction = stopfunction;
	kth->object = object;

	kobj_makechild(&kth->kobject, kobj_cast(trigger));
	kobj_makechild(&kth->kobject, object);
	return kth;
}

kthread_t * kthread_self()
{
	return kthread_local;
}

static bool kthread_dotasks(mainloop_t * loop, uint64_t nanoseconds, void * userdata)
{
	unused(loop);
	unused(nanoseconds);
	unused(userdata);

	mutex_lock(&kthreads_mutex);
	{
		list_t * pos = NULL, * q = NULL;
		list_foreach_safe(pos, q, &kthreads)
		{
			kthread_t * kth = list_entry(pos, kthread_t, schedule_list);
			list_remove(&kth->schedule_list);

			const char * object_name = NULL;
			kobj_getinfo(&kth->kobject, NULL, &object_name, NULL);
			LOGK(LOG_DEBUG, "Starting thread %s", object_name);

			kthread_start(kth);
		}
	}
	mutex_unlock(&kthreads_mutex);
	return true;
}

static bool kthread_dopass(kthread_t * thread, kobject_t * object)
{
	kthreaddata_t * data = (kthreaddata_t *)object;
	return data->run(data->userdata);
}

static bool kthread_dostop(kthread_t * thread, kobject_t * object)
{
	kthreaddata_t * data = (kthreaddata_t *)object;
	return data->stop(data->userdata);
}

bool kthread_newinterval(const char * name, int priority, double rate_hz, handler_f threadfunc, void * userdata, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(name == NULL || rate_hz <= 0 || threadfunc == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}
	}

	kthreaddata_t * data = kobj_new("Thread Data", name, NULL, NULL, sizeof(kthreaddata_t));
	data->run = threadfunc;
	data->stop = NULL;
	data->userdata = userdata;

	trigger_clock_t * trigger = trigger_newclock(name, rate_hz);
	kthread_t * kth = kthread_new(name, priority, trigger_cast(trigger), kobj_cast(data), kthread_dopass, NULL, err);
	if (kth == NULL || exception_check(err))
	{
		return false;
	}

	kthread_schedule(kth);
	return true;
}

bool kthread_newthread(const char * name, int priority, handler_f threadfunc, handler_f stopfunc, void * userdata, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(name == NULL || threadfunc == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}
	}

	kthreaddata_t * data = kobj_new("Thread Data", name, NULL, NULL, sizeof(kthreaddata_t));
	data->run = threadfunc;
	data->stop = stopfunc;
	data->userdata = userdata;

	kthread_t * kth = kthread_new(name, priority, NULL, kobj_cast(data), kthread_dopass, kthread_dostop, err);
	if (kth == NULL || exception_check(err))
	{
		return false;
	}

	kthread_schedule(kth);
	return true;
}

// --------------------- Kernel functions -----------------------
const char * max_model() { return property_get("model"); }
const char * kernel_id() { return property_get("id"); }
int kernel_installed()
{
	const char * installed = property_get("installed");
	return (installed != NULL)? parse_int(installed, NULL) : 0;
}

// Microseconds since epoch
int64_t kernel_timestamp()
{
	static struct timeval now;
	gettimeofday(&now, NULL);

	uint64_t ts = (uint64_t)now.tv_sec * 1000000LL;
	ts += now.tv_usec;

	return ts;
}

// Microseconds since kernel startup
int64_t kernel_elapsed()
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
	const void * mods_next(const void * object, void ** itrobject)
	{
		unused(object);

		list_t * list = *itrobject = ((list_t *)*itrobject)->next;

		if (list == &modules)
		{
			return NULL;
		}
		else
		{
			module_t * module = list_entry(list, module_t, global_list);

			if (module->backing != NULL)
			{
				const char * path = NULL;
				meta_getinfo(module->backing, &path, NULL, NULL, NULL, NULL);
				return path;
			}

			return NULL;
		}
	}

	return iterator_new("modules", mods_next, NULL, NULL, &modules);
}

static const char * modules_next(int itr)
{
	const char * r = iterator_next(itr, "modules");
	return (r == NULL)? "" : r;
}

static int syscalls_itr()
{
	const void * scs_next(const void * object, void ** itrobject)
	{
		unused(object);

		list_t * list = *itrobject = ((list_t *)*itrobject)->next;
		return (list == hashtable_itr(&syscalls))? NULL : hashtable_itrentry(list, syscall_t, global_entry)->name;
	}

	return iterator_new("syscalls", scs_next, NULL, NULL, hashtable_itr(&syscalls));
}

static const char * syscalls_next(int itr)
{
	const char * r = iterator_next(itr, "syscalls");
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
	const void * ps_next(const void * object, void ** itrobject)
	{
		unused(object);

		list_t * list = *itrobject = ((list_t *)*itrobject)->next;
		return (list == hashtable_itr(&properties))? NULL : hashtable_itrentry(list, property_t, entry)->name;
	}

	return iterator_new("properties", ps_next, NULL, NULL, hashtable_itr(&properties));
}

static const char * properties_next(int itr)
{
	const char * r = iterator_next(itr, "properties");
	return (r == NULL)? "" : r;
}

static void itr_free(int itr)
{
	iterator_free(itr);
}


// --------------------- Main function -----------------------
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

		if (sizeof(bool) != 1)
		{
			LOGK(LOG_FATAL, "Failed bool size runtime check!");
		}

		// TODO IMPORTANT - check for threadlocal support
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

	// Initialize global variables
	model = model_new();
	LIST_INIT(&modules);
	LIST_INIT(&rategroups);
	LIST_INIT(&kobjects);
	LIST_INIT(&kthreads);
	HASHTABLE_INIT(&properties, hash_str, hash_streq);
	HASHTABLE_INIT(&syscalls, hash_str, hash_streq);
	mutex_init(&kobj_mutex, M_RECURSIVE);
	mutex_init(&kthreads_mutex, M_RECURSIVE);
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
			if (write(pfd, data.string, data.length) != (ssize_t)data.length)
			{
				LOGK(LOG_WARN, "Could not write all data to pid file %s", PIDFILE);
			}
			close(pfd);
		}
	}

	// Init AUL components
	mainloop_init();
	iterator_init();

	// Initialize kernel subsystems
	cal_init();
	//module_kernelinit();	//a generic module that points to kernel space
	
	// Register syscalls
	#define reg_syscall(f, s, d) \
		({ \
			exception_t * e = NULL;												\
			syscall_t * ts = syscall_new((#f), (s), (syscall_f)(f), (d), &e);	\
			if (ts == NULL || exception_check(&e)) {							\
				LOGK(LOG_ERR, "Could not create kernel syscall %s: %s", (#f), (e == NULL)? "Unknown error" : e->message); \
				exception_free(e);												\
			}																	\
		})

	reg_syscall(	modules_itr,		"i:v",		"Returns an iterator that loops over all loaded modules. Use in conjunction with 'modules_next' and 'itr_free'");
	reg_syscall(	modules_next,		"s:i",		"Returns the next name of module in iterator. Use in conjunction with 'modules_itr' and 'itr_free'");
	reg_syscall(	module_exists,		"b:s",		"Returns true if module exists and is loaded by name (param 1)");
	reg_syscall(	syscalls_itr,		"i:v",		"Returns an iterator that loop over all registered syscalls. Use in conjunction with 'syscalls_next' and 'itr_free'");
	reg_syscall(	syscalls_next,		"s:i",		"Returns the next name of the syscall in the iterator. Use in conjunction with 'syscalls_itr' and 'itr_free'");
	reg_syscall(	syscall_info,		"s:s",		"Returns description of the given syscall (param 1)");
	reg_syscall(	syscall_exists,		"b:ss",		"Returns true if syscall exists by name (param 1) and signature (param 2). If signature is an empty string or null, only name is evaluated");
	reg_syscall(	syscall_signature,	"s:s",		"Returns the signature for the given syscall (param 1) if it exists, or an empty string if not");
	reg_syscall(	max_model,			"s:v",		"Returns the model name of the robot");
	reg_syscall(	kernel_id,			"s:v",		"Returns the unique id of the kernel (non-volatile)");
	reg_syscall(	kernel_installed,	"i:v",		"Returns a unix timestamp when maxkernel was installed");
	reg_syscall(	properties_itr,		"i:v",		"Returns an iterator that loops over all the property names defined. Use in conjunction with 'properties_next' and 'itr_free'");
	reg_syscall(	properties_next,	"s:i",		"Returns the next property name in the iterator. Use in conjunction with 'properties_itr' and 'itr_free'");
	reg_syscall(	property_get,		"s:s",		"Returns the string representation of the defined property");
	reg_syscall(	property_set,		"v:ss",		"Sets the property name (param 1) to the specified value (param 2)");
	reg_syscall(	property_clear,		"v:s",		"Clears the property name (param 1) and the associated value from the database");
	reg_syscall(	property_isset,		"b:s",		"Returns true if the property name (param 1) has been set");
	reg_syscall(    itr_free,			"v:i",		"Frees the given iterator. It can no longer be used after it's free'd");

	// Parse configuration file
	{
		const char * scriptpath = INSTALL "/" CONFIG;
		model_script_t * script = NULL;

		{
			exception_t * e = NULL;
			script = model_newscript(model, scriptpath, &e);

			if (script == NULL || exception_check(&e))
			{
				LOGK(LOG_FATAL, "Could not create script '%s' in model: %s", scriptpath, (e == NULL)? "Unknown error" : e->message);
				// Will exit
			}
		}

		// These are the interpreter callback functions
		void int_log(level_t level, const char * message)
		{
			log_write(level, "Script", "%s", message);
		}

		meta_t * int_metalookup(const char * modulename, exception_t ** err)
		{
			const char * prefix = path_resolve(modulename, P_MODULE);
			if (prefix == NULL)
			{
				exception_set(err, ENOENT, "Could not resolve path %s", modulename);
				return NULL;
			}

			string_t path = path_join(prefix, modulename);
			if (!strsuffix(path.string, ".mo"))
			{
				string_append(&path, ".mo");
			}

			return meta_parseelf(path.string, err);
		}

		bool int_setpath(const char * newpath, exception_t ** err)	{	return path_set(newpath, err);		}
		bool int_appendpath(const char * path, exception_t ** err)	{	return path_append(path, err);		}


		// These are the script callback functions
		int cfg_log(cfg_t * cfg, level_t level, int argc, const char ** argv)
		{
			if (argc < 1)
			{
				cfg_error(cfg, "Invalid argument length for log function");
				return -1;
			}

			for (int i = 0; i < argc; i++)
			{
				int_log(level, argv[i]);
			}

			return 0;
		}

		int cfg_loginfo(cfg_t * cfg, cfg_opt_t * opt, int argc, const char ** argv)
		{
			unused(opt);

			return cfg_log(cfg, LOG_INFO, argc, argv);
		}

		int cfg_logwarn(cfg_t * cfg, cfg_opt_t * opt, int argc, const char ** argv)
		{
			unused(opt);

			return cfg_log(cfg, LOG_WARN, argc, argv);
		}

		int cfg_logerror(cfg_t * cfg, cfg_opt_t * opt, int argc, const char ** argv)
		{
			unused(opt);

			cfg_log(cfg, LOG_ERR, argc, argv);
			cfg_error(cfg, "Script aborted.");
			return -1;
		}

		int cfg_setpath(cfg_t * cfg, cfg_opt_t * opt, int argc, const char ** argv)
		{
			unused(opt);

			if (argc != 1)
			{
				cfg_error(cfg, "Invalid argument length for setpath function");
				return -1;
			}

			exception_t * e = NULL;
			if (!int_setpath(argv[0], &e))
			{
				cfg_error(cfg, "Could not set path: Code %d %s", e->code, e->message);
				exception_free(e);
				return -1;
			}

			return 0;
		}

		int cfg_appendpath(cfg_t * cfg, cfg_opt_t * opt, int argc, const char ** argv)
		{
			unused(opt);

			if (argc < 1)
			{
				cfg_error(cfg, "Invalid argument length for appendpath function");
				return -1;
			}

			exception_t * e = NULL;
			for (int i = 0; i < argc; i++)
			{
				if (!int_appendpath(argv[i], &e))
				{
					cfg_error(cfg, "Could not append path: Code %d %s", e->code, e->message);
					exception_free(e);
					return -1;
				}
			}

			return 0;
		}

		int cfg_loadmodule(cfg_t * cfg, cfg_opt_t * opt, int argc, const char ** argv)
		{
			unused(opt);

			if (argc < 1)
			{
				cfg_error(cfg, "Invalid argument length for loadmodule function");
				return -1;
			}

			for (int i = 0; i < argc; i++)
			{
				exception_t * e = NULL;
				meta_t * meta = int_metalookup(argv[i], &e);
				if (meta == NULL || exception_check(&e))
				{
					cfg_error(cfg, "Could not load module %s: %s", argv[i], (e == NULL)? "Unknown error" : e->message);
					exception_free(e);
					return -1;
				}

				model_module_t * module = model_newmodule(model, script, meta, &e);
				if (module == NULL || exception_check(&e))
				{
					cfg_error(cfg, "Could not add module %s: %s", argv[i], (e == NULL)? "Unknown error" : e->message);
					exception_free(e);
					return -1;
				}

				meta_destroy(meta);
			}

			return 0;
		}

		int cfg_config(cfg_t * cfg, cfg_opt_t * opt, int argc, const char ** argv)
		{
			unused(opt);

			if (argc != 3)
			{
				cfg_error(cfg, "Invalid argument length for config function");
				return -1;
			}

			const char * modulename = argv[0];
			const char * name = argv[1];
			const char * value = argv[2];

			const char * prefix = path_resolve(modulename, P_MODULE);
			if (prefix == NULL)
			{
				cfg_error(cfg, "Could not resolve path %s", modulename);
				return -1;
			}

			string_t path = path_join(prefix, modulename);
			if (!strsuffix(path.string, ".mo"))
			{
				string_append(&path, ".mo");
			}

			model_module_t * module = NULL;
			if (!model_lookupmodule(model, script, path.string, &module))
			{
				cfg_error(cfg, "Could not find module %s. Module not loaded?", modulename);
				return -1;
			}

			exception_t * e = NULL;
			model_config_t * param = model_newconfig(model, module, name, value, &e);
			if (param == NULL || exception_check(&e))
			{
				cfg_error(cfg, "Could not set config %s=%s: %s", name, value, (e == NULL)? "Unknown error" : e->message);
				exception_free(e);
				return -1;
			}

			return 0;
		}

		int cfg_execfile(cfg_t * cfg, cfg_opt_t * opt, int argc, const char ** argv)
		{
			unused(opt);

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

				interpret_f execfunc = NULL;
				if (strsuffix(path.string, ".lua"))
				{
					execfunc = interpret_lua;
				}
				else
				{
					LOGK(LOG_INFO, "Could not determine how interpret file %s. Skipping.", path.string);
					continue;
				}

				interpret_callbacks cbs = {
					.log			= int_log,
					.metalookup		= int_metalookup,
					.setpath		= int_setpath,
					.appendpath		= int_appendpath,
				};

				exception_t * e = NULL;
				if (!execfunc(model, path.string, &cbs, &e))
				{
					// TODO - make less verbose?
					cfg_error(cfg, "Error while executing script: %s", (e == NULL)? "Unknown error" : e->message);
					exception_free(e);
					return -1;
				}
			}

			return 0;
		}

		int cfg_execdir(cfg_t * cfg, cfg_opt_t * opt, int argc, const char ** argv)
		{
			unused(opt);

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

				// TODO - use walkdir?
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

		void cfg_errorfunc(cfg_t * cfg, const char * fmt, va_list ap)
		{
			string_t str = string_blank();
			string_vset(&str, fmt, ap);
			LOGK(LOG_FATAL, "%s", str.string);
		}

		cfg_opt_t opts[] = {
			CFG_STR(	"id",			"(none)",				CFGF_NONE	),
			CFG_STR(	"path",			INSTALL "/modules",		CFGF_NONE	),
			CFG_STR(	"installed",	"0",					CFGF_NONE	),
			CFG_STR(	"model",		"(unknown)",			CFGF_NONE	),
			CFG_FUNC(	"log",			cfg_loginfo				),
			CFG_FUNC(	"print",		cfg_loginfo				),
			CFG_FUNC(	"warn",			cfg_logwarn				),
			CFG_FUNC(	"error",		cfg_logerror			),
			CFG_FUNC(	"setpath",		cfg_setpath				),
			CFG_FUNC(	"appendpath",	cfg_appendpath			),
			CFG_FUNC(	"loadmodule",	cfg_loadmodule			),
			CFG_FUNC(	"config",		cfg_config				),
			CFG_FUNC(	"execfile",		cfg_execfile			),
			CFG_FUNC(	"execdirectory",cfg_execdir				),
			CFG_END()
		};

		// Create the configuration scruct
		cfg_t * cfg = cfg_init(opts, 0);
		cfg_set_error_function(cfg, cfg_errorfunc);
	
		// Parse/execute the script
		LOGK(LOG_DEBUG, "Parsing configuration script %s", scriptpath);
		int result = cfg_parse(cfg, scriptpath);
		if (result != CFG_SUCCESS)
		{
			switch (result)
			{
				case CFG_FILE_ERROR:
					LOGK(LOG_FATAL, "Could not open configuration file %s", scriptpath);

				case CFG_PARSE_ERROR:
					LOGK(LOG_FATAL, "Parse error in configuration file %s", scriptpath);

				default:
					LOGK(LOG_FATAL, "Unknown error while opening/parsing configuration file %s", scriptpath);
			}
		}

		// Register global properties
		property_set("id", cfg_getstr(cfg, "id"));
		property_set("installed", cfg_getstr(cfg, "installed"));
		property_set("model", cfg_getstr(cfg, "model"));

		// Free the configuration struct
		cfg_free(cfg);
	}

	// Now start building and instantiating the kernel structure from the model
	{
		// Load the modules
		{
			void * f_load_modules(void * udata, const model_t * model, const model_module_t * module, const model_script_t * script)
			{
				// Sanity check
				{
					if unlikely(udata != NULL)
					{
						LOGK(LOG_WARN, "Module model userdata already set! (Will overwrite)");
					}
				}

				meta_t * meta_lookup(const char * filename)
				{
					LOGK(LOG_DEBUG, "Resolving dependency %s", filename);

					const char * prefix = path_resolve(filename, P_MODULE);
					if (prefix == NULL)
					{
						return NULL;
					}

					string_t path = path_join(prefix, filename);
					if (!strsuffix(path.string, ".mo"))
					{
						string_append(&path, ".mo");
					}

					const meta_t * meta = NULL;
					if (!model_lookupmeta(model, path.string, &meta))
					{
						// Meta not in model, load it in manually
						exception_t * e = NULL;
						meta = meta_parseelf(path.string, &e);
						if (meta == NULL || exception_check(&e))
						{
							LOGK(LOG_ERR, "Load meta failed: %s", (e == NULL)? "Unknown error!" : e->message);
							exception_free(e);
							return NULL;
						}
					}

					// Remove const qualifier (see below)
					return (meta_t *)meta;
				}

				const meta_t * meta = NULL;
				model_getmodule(module, NULL, &meta);
				if (meta == NULL)
				{
					LOGK(LOG_FATAL, "Could not get module meta from model");
					// Will exit
				}

				exception_t * e = NULL;

				// Loading the module *will* change the underlying struct (ptrs will resolve),
				// so we must cast to get rid of const qualifier
				module_t * m = module_load((model_t *)model, (meta_t *)meta, meta_lookup, &e);
				if (m == NULL || exception_check(&e))
				{
					LOGK(LOG_FATAL, "Module loading failed: %s", (e == NULL)? "Unknown error" : e->message);
					// Will exit
				}

				return m;
			}

			model_analysis_t f_load = {
				.modules = f_load_modules,
			};

			LOGK(LOG_DEBUG, "Loading kernel modules...");
			model_analyse(model, &f_load);
		}

		// Build the kernel elements
		{
			void * f_build_configs(void * udata, const model_t * model, const model_config_t * config, const model_script_t * script, const model_module_t * module)
			{
				// Sanity check
				{
					if unlikely(udata != NULL)
					{
						LOGK(LOG_WARN, "Config model userdata already set! (Will overwrite)");
					}
				}

				const meta_t * meta = NULL;
				model_getmodule(module, NULL, &meta);

				const char * path = NULL;
				meta_getinfo(meta, &path, NULL, NULL, NULL, NULL);

				const char * name = NULL;
				model_getconfig(config, &name, NULL, NULL, NULL);

				module_t * mod = module_lookup(path);
				if (mod == NULL)
				{
					LOGK(LOG_FATAL, "Could not apply config %s to module %s, module doesn't exist!", name, path);
					// Will exit
				}

				list_t * pos = NULL;
				config_t * modconfig = NULL;
				list_foreach(pos, &mod->configs)
				{
					config_t * test = list_entry(pos, config_t, module_list);
					const char * testname = NULL;
					meta_getvariable(test->variable, &testname, NULL, NULL, NULL);

					if (strcmp(name, testname) == 0)
					{
						modconfig = test;
						break;
					}
				}

				if (modconfig == NULL)
				{
					LOGK(LOG_FATAL, "Could not apply config %s to module %s, config item doesn't exist!", name, path);
					// Will exit
				}

				exception_t * e = NULL;
				if (!config_apply(modconfig, config, &e))
				{
					LOGK(LOG_FATAL, "Could not apply config %s: %s", name, (e == NULL)? "Unknown error" : e->message);
					// Will exit
				}

				return modconfig;
			}

			void * f_build_blockinsts(void * udata, const model_t * model, const model_linkable_t * linkable, const model_blockinst_t * blockinst, const model_script_t * script)
			{
				// Sanity check
				{
					if unlikely(udata != NULL)
					{
						LOGK(LOG_WARN, "Blockinst model userdata already set! (Will overwrite)");
					}
				}

				const char * block_name = NULL;
				const model_module_t * module = NULL;
				const char * sig = NULL;
				const char * const * args = NULL;
				size_t argslen = 0;
				model_getblockinst(linkable, &block_name, &module, &sig, &args, &argslen);

				const char * module_path = NULL;
				const meta_t * meta = NULL;
				model_getmodule(module, &module_path, &meta);

				const char * module_name = NULL;
				meta_getinfo(meta, NULL, &module_name, NULL, NULL, NULL);

				LOGK(LOG_DEBUG, "Creating block instance '%s' in module %s", block_name, module_path);

				if (method_numparams(sig) != argslen)
				{
					LOGK(LOG_FATAL, "Bad constructor arguments given to block '%s' in module %s", block_name, module_path);
					// Will exit
				}

				string_t name = string_new("%s.%s", module_name, block_name);

				module_t * m = model_userdata(model_object(module));
				block_t * b = module_lookupblock(m, block_name);

				exception_t * e = NULL;
				blockinst_t * bi = blockinst_new(b, name.string, args, &e);
				if (bi == NULL || exception_check(&e))
				{
					LOGK(LOG_FATAL, "Could not create block '%s': %s", block_name, (e == NULL)? "Unknown error" : e->message);
					// Will exit
				}

				return bi;
			}

			void * f_build_syscalls(void * udata, const model_t * model, const model_linkable_t * linkable, const model_syscall_t * syscall, const model_script_t * script)
			{
				// Sanity check
				{
					if unlikely(udata != NULL)
					{
						LOGK(LOG_WARN, "Syscall model userdata already set! (Will overwrite)");
					}
				}

				const char * name = NULL, * sig = NULL, * desc = NULL;
				model_getsyscall(linkable, &name, &sig, &desc);

				LOGK(LOG_DEBUG, "Creating syscall %s(%s)", name, sig);

				exception_t * e = NULL;
				syscallblock_t * sb = syscallblock_new(name, sig, desc, &e);
				if (sb == NULL || exception_check(&e))
				{
					LOGK(LOG_FATAL, "Could not create syscall %s(%s): %s", name, sig, (e == NULL)? "Unknown error" : e->message);
					// Will exit
				}

				return sb;
			}

			void * f_build_rategroups(void * udata, const model_t * model, const model_linkable_t * linkable, const model_rategroup_t * rategroup, const model_script_t * script)
			{
				// Sanity check
				{
					if unlikely(udata != NULL)
					{
						LOGK(LOG_WARN, "Rategroup model userdata already set! (Will overwrite)");
					}
				}

				const char * name = NULL;
				double hertz = 0.0;
				model_getrategroup(linkable, &name, &hertz);

				LOGK(LOG_DEBUG, "Creating rategroup %s with update at %f Hz", name, hertz);

				rategroup_t * rg = NULL;

				// Create the rategroup
				{
					exception_t * e = NULL;
					rg = rategroup_new(linkable, &e);
					if (rg == NULL || exception_check(&e))
					{
						LOGK(LOG_FATAL, "Could not create rategroup %s at %f Hz: %s", name, hertz, (e == NULL)? "Unknown error" : e->message);
						// Will exit
					}
				}

				// Iterator over the blockinsts and add them to the rategroup
				{
					iterator_t bitr = model_rategroupblockinstitr(model, linkable);
					{
						const model_linkable_t * blockinst = NULL;
						while (model_rategroupblockinstnext(bitr, &blockinst))
						{
							blockinst_t * bi = model_userdata(model_object(blockinst));

							exception_t * e = NULL;
							bool success = rategroup_addblockinst(rg, bi, &e);
							if (!success || exception_check(&e))
							{
								LOGK(LOG_FATAL, "Could not add block instance to rategroup '%s'", name);
								// Will exit
							}
						}
					}
					iterator_free(bitr);
				}

				return rg;
			}

			void * f_build_links(void * udata, const model_t * model, const model_link_t * link, const model_linksymbol_t * out, const model_linksymbol_t * in, const model_script_t * script)
			{
				// Sanity check
				{
					if unlikely(udata != NULL)
					{
						LOGK(LOG_WARN, "Link model userdata already set! (Will overwrite)");
					}
				}

				// Get the information about the like
				const model_linkable_t * out_linkable = NULL, * in_linkable = NULL;
				const char * out_name = NULL, * in_name = NULL;
				model_getlinksymbol(out, &out_linkable, &out_name, NULL, NULL);
				model_getlinksymbol(in, &in_linkable, &in_name, NULL, NULL);

				// Find this information
				char out_sig = 0, in_sig = 0;
				linklist_t * out_links = NULL, * in_links = NULL;

				// Get the out link info
				{
					switch (model_type(model_object(out_linkable)))
					{
						case model_blockinst:
						{
							blockinst_t * bi = model_userdata(model_object(out_linkable));
							char sig = 0;

							block_iolookup(blockinst_block(bi), out_name, meta_output, &sig, NULL);
							if (sig == 0)
							{
								LOGK(LOG_FATAL, "Could not find output '%s' in block", out_name);
								// Will exit
							}

							out_sig = sig;
							out_links = blockinst_links(bi);
							break;
						}

						case model_syscall:
						{
							syscallblock_t * sb = model_userdata(model_object(out_linkable));
							port_t * port = port_lookup(syscallblock_ports(sb), meta_output, out_name);
							if (port == NULL)
							{
								LOGK(LOG_FATAL, "Output port '%s' in syscall block instance doesn't exist!", out_name);
								// Will exit
							}

							out_sig = iobacking_sig(port_iobacking(port));
							out_links = syscallblock_links(sb);
							break;
						}

						case model_rategroup:
						{
							rategroup_t * rg = model_userdata(model_object(out_linkable));
							port_t * port = port_lookup(rategroup_ports(rg), meta_output, out_name);
							if (port == NULL)
							{
								LOGK(LOG_FATAL, "Output port '%s' in rategroup doesn't exist!", out_name);
								// Will exit
							}

							out_sig = iobacking_sig(port_iobacking(port));
							out_links = rategroup_links(rg);
							break;
						}

						default: break;
					}

					if (out_sig == 0 || out_links == NULL)
					{
						LOGK(LOG_FATAL, "Could not connect output link %s -> %s, bad types!", out_name, in_name);
						// Will exit
					}
				}

				// Get the in link info
				{
					switch (model_type(model_object(in_linkable)))
					{
						case model_blockinst:
						{
							blockinst_t * bi = model_userdata(model_object(in_linkable));
							char sig = 0;

							block_iolookup(blockinst_block(bi), in_name, meta_input, &sig, NULL);
							if (sig == 0)
							{
								LOGK(LOG_FATAL, "Could not find input '%s' in block", in_name);
								// Will exit
							}

							in_sig = sig;
							in_links = blockinst_links(bi);
							break;
						}

						case model_syscall:
						{
							syscallblock_t * sb = model_userdata(model_object(in_linkable));
							port_t * port = port_lookup(syscallblock_ports(sb), meta_input, in_name);
							if (port == NULL)
							{
								LOGK(LOG_FATAL, "Input port '%s' in syscall doesn't exist!", in_name);
								// Will exit
							}

							in_sig = iobacking_sig(port_iobacking(port));
							in_links = syscallblock_links(sb);
							break;
						}

						case model_rategroup:
						{
							rategroup_t * rg = model_userdata(model_object(in_linkable));
							port_t * port = port_lookup(rategroup_ports(rg), meta_input, in_name);
							if (port == NULL)
							{
								LOGK(LOG_FATAL, "Input port '%s' in rategroup doesn't exist!", in_name);
								// Will exit
							}

							in_sig = iobacking_sig(port_iobacking(port));
							in_links = rategroup_links(rg);
							break;
						}

						default: break;
					}

					if (in_sig == 0 || in_links == NULL)
					{
						LOGK(LOG_FATAL, "Could not connect input link %s -> %s, bad types!", out_name, in_name);
						// Will exit
					}
				}

				exception_t * e = NULL;
				iobacking_t * iob = link_connect(link, out_sig, out_links, in_sig, in_links, &e);
				if (iob == NULL || exception_check(&e))
				{
					LOGK(LOG_FATAL, "Could not connect link from %s -> %s: %s", out_name, in_name, (e == NULL)? "Unknown error" : e->message);
					// Will exit
				}

				return iob;
			}


			// Call preact function on all modules
			{
				list_t * pos;
				list_foreach(pos, &modules)
				{
					module_t * module = list_entry(pos, module_t, global_list);
					module_activate(module, act_preact);
				}
			}

			model_analysis_t f_build1 = {
				.configs = f_build_configs,
				.blockinsts = f_build_blockinsts,
				.syscalls = f_build_syscalls,
			};

			model_analysis_t f_build2 = {
				.rategroups = f_build_rategroups,
			};

			model_analysis_t f_build3 = {
				.links = f_build_links,
			};

			LOGK(LOG_DEBUG, "Building the kernel structure...");
			model_analyse(model, &f_build1);
			model_analyse(model, &f_build2);
			model_analyse(model, &f_build3);
		}

		// Create the kernel objects
		{
			void * f_create_blockinsts(void * udata, const model_t * model, const model_linkable_t * linkable, const model_blockinst_t * blockinst, const model_script_t * script)
			{
				// Sanity check
				{
					if unlikely(udata == NULL)
					{
						LOGK(LOG_FATAL, "Block instance model userdata not set!");
						// Will exit
					}
				}

				blockinst_t * bi = udata;
				LOGK(LOG_DEBUG, "Creating block instance %s", block_name(blockinst_block(bi)));

				exception_t * e = NULL;
				bool success = blockinst_create(bi, &e);
				if (!success || exception_check(&e))
				{
					LOGK(LOG_FATAL, "Could not create block instance: %s", (e == NULL)? "Unknown error" : e->message);
					// Will exit
				}

				return udata;
			}

			void * f_create_rategroups(void * udata, const model_t * model, const model_linkable_t * linkable, const model_rategroup_t * rategroup, const model_script_t * script)
			{
				rategroup_t * rg = udata;
				LOGK(LOG_DEBUG, "Scheduling rategroup %s", rategroup_name(rg));

				exception_t * e = NULL;
				bool success = rategroup_schedule(rg, RATEGROUP_PRIO, &e);
				if (!success || exception_check(&e))
				{
					LOGK(LOG_FATAL, "Could not create rategroup: %s", (e == NULL)? "Unknown error" : e->message);
					// Will exit
				}

				return udata;
			}


			model_analysis_t f_create1 = {
				.blockinsts = f_create_blockinsts,
			};

			model_analysis_t f_create2 = {
				.rategroups = f_create_rategroups,
			};

			LOGK(LOG_DEBUG, "Creating the kernel objects...");
			model_analyse(model, &f_create1);
			model_analyse(model, &f_create2);
		}

		// Call postact function on all modules
		{
			list_t * pos;
			list_foreach(pos, &modules)
			{
				module_t * module = list_entry(pos, module_t, global_list);
				module_activate(module, act_postact);
			}
		}
	}

	// TODO - Verify the kernel configuration
	{
		//analyze inputs and outputs on all blocks and make sure everything is kosher
		//now loop through all kthreads and delete each block_inst_t from the list
		//now iterate over list and warn on each non-null value
	}

	// Run post-build functions
	{
		// Sort the modules
		{
			int module_compare(list_t * a, list_t * b)
			{
				module_t * ma = list_entry(a, module_t, global_list);
				module_t * mb = list_entry(b, module_t, global_list);
				return strcmp(ma->kobject.object_name, mb->kobject.object_name);
			}

			list_sort(&modules, module_compare);
		}

		// Sort the calibration entries
		{
			cal_sort();
		}

		// Call init function on all modules
		{
			list_t * pos;
			list_foreach(pos, &modules)
			{
				module_t * module = list_entry(pos, module_t, global_list);
				module_init(module);
			}
		}

		// Check for new kernel thread tasks every second
		mainloop_addtimer(NULL, "KThread task handler", NANOS_PER_SECOND, kthread_dotasks, NULL);
	}

	// Run maxkernel mainloop
	mainloop_run(NULL);

	// Returning from the main loop means we've gotten a signal to exit
	LOGK(LOG_DEBUG, "Destroying the kernel.");
	{
		// Destroy the kobjects
		mutex_lock(&kobj_mutex);
		{

			while (kobjects.prev != &kobjects)
			{
				kobject_t * item = NULL;
				bool found = false;

				list_t * pos;
				list_foreach_reverse(pos, &kobjects)
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
	}

	LOGK(LOG_INFO, "MaxKernel Exit.");

	// Destroy the log subsystem
	log_destroy();

	// Goodbye
	return 0;
}
