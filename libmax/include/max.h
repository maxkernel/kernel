#ifndef __MAX_H
#define __MAX_H

#include <stdarg.h>
#include <stdbool.h>

#include <aul/common.h>
#include <aul/exception.h>
#include <aul/hashtable.h>
#include <aul/mutex.h>


#ifdef __cplusplus
extern "C" {
#endif


#define T_METHOD		'M'
#define T_ERROR			'E'
#define T_RETURN		'R'
#define T_VOID			'v'
#define T_BOOLEAN		'b'
#define T_INTEGER		'i'
#define T_DOUBLE		'd'
#define T_CHAR			'c'
#define T_STRING		's'

#define HOST_UNIXSOCK	"unix:"
#define HOST_IP			"ip:"
#define HOST_UID		"uid:"

#define DEFAULT_TIMEOUT	100

typedef void * (*malloc_f)(size_t size);
typedef void (*free_f)(void * ptr);
typedef void (*memerr_f)();

typedef struct
{
	malloc_f malloc;
	free_f free;
	memerr_f memerr;

	hashtable_t syscalls;

	int sock;
	mutex_t sock_mutex;
	int timeout;
} maxhandle_t;

typedef struct
{
	maxhandle_t * handle;

	char type;
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


void max_init(maxhandle_t * hand);
void max_setmalloc(maxhandle_t * hand, malloc_f mfunc, free_f ffunc, memerr_f efunc);
void max_memerr();

bool max_connect(maxhandle_t * hand, exception_t ** err, const char * host);
bool max_connectlocal(maxhandle_t * hand, exception_t ** err);
void max_close(maxhandle_t * hand);

string_t max_syscallsig(maxhandle_t * hand, const char * name);
bool max_syscallcache(maxhandle_t * hand, const char * name, const char * sig);
bool max_syscallexists(maxhandle_t * hand, const char * name, const char * sig);

bool max_syscall(maxhandle_t * hand, exception_t ** err, return_t * ret, const char * syscall, const char * sig, ...);
bool max_vsyscall(maxhandle_t * hand, exception_t ** err, return_t * ret, const char * syscall, const char * sig, va_list args);
//return_t max_asyscall(maxhandle_t * hand, const char * syscall, const char * sig, void ** args);

void max_settimeout(maxhandle_t * hand, int newtimeout);
int max_gettimeout(maxhandle_t * hand);

#ifdef __cplusplus
}
#endif
#endif
