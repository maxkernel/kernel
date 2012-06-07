#ifndef __MAX_H
#define __MAX_H

#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

#include <aul/exception.h>
#include <aul/hashtable.h>
#include <aul/mutex.h>
#include <aul/parse.h>

#include <kernel-types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HOST_LOCAL			"local:"
#define HOST_UNIXSOCK		"unix:"
#define HOST_IP				"ip:"
#define HOST_UID			"uid:"

#define DEFAULT_TIMEOUT		100

#define SYSCALL_CACHE_SIZE			75
#define SYSCALL_CACHE_NAMELEN		50
#define SYSCALL_CACHE_SIGLEN		10		// TODO - merge this with CONSOLE_HEADERSIZE


typedef void * (*malloc_f)(size_t size);
typedef void (*free_f)(void * ptr);
typedef void (*memerr_f)();


typedef struct
{
	char name[SYSCALL_CACHE_NAMELEN];
	char sig[SYSCALL_CACHE_SIGLEN];
	//const char * description;		// TODO - support description in syscall cache
	time_t last_access;

	hashentry_t syscalls_entry;
	list_t syscalls_list;
} syscall_t;

typedef struct
{
	malloc_f malloc;
	free_f free;
	memerr_f memerr;

	void * syscall_cache;

	int sock;
	mutex_t sock_mutex;

	int timeout;
	void * userdata;
} maxhandle_t;

typedef struct
{
	maxhandle_t * handle;

	char type;
	char sig;
	union
	{
		bool t_boolean;
		int t_integer;
		double t_double;
		char t_char;
		char t_string[AUL_STRING_MAXLEN];
		exception_t error;	// TODO - support returning exceptions?
	} data;
} return_t;


void max_initialize(maxhandle_t * hand);
void max_setmalloc(maxhandle_t * hand, malloc_f mfunc, free_f ffunc, memerr_f efunc);
void max_memerr();

bool max_connect(maxhandle_t * hand, const char * host, exception_t ** err);
bool max_connectlocal(maxhandle_t * hand, exception_t ** err);
void * max_destroy(maxhandle_t * hand);

bool max_syscall(maxhandle_t * hand, exception_t ** err, return_t * ret, const char * syscall, const char * sig, ...);
bool max_vsyscall(maxhandle_t * hand, exception_t ** err, return_t * ret, const char * syscall, const char * sig, va_list args);
bool max_asyscall(maxhandle_t * hand, exception_t ** err, return_t * ret, const char * syscall, const char * sig, void ** args);

void max_syscallcache_enable(maxhandle_t * hand);
void max_syscallcache_destroy(maxhandle_t * hand);
syscall_t * max_syscallcache_lookup(maxhandle_t * hand, exception_t ** err, const char * name);
bool max_syscallcache_exists(maxhandle_t * hand, const char * name, const char * sig);
const char * max_syscallcache_getsig(maxhandle_t * hand, const char * name);

void max_settimeout(maxhandle_t * hand, int newtimeout);
int max_gettimeout(maxhandle_t * hand);
void max_setuserdata(maxhandle_t * hand, void * userdata);
void * max_getuserdata(maxhandle_t * hand);

// TODO - make some of these #defines or at lease inlines
size_t max_getbuffersize(maxhandle_t * hand);
size_t max_getheadersize(maxhandle_t * hand);

#ifdef __cplusplus
}
#endif
#endif
