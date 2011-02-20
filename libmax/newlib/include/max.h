#ifndef __MAX_H
#define __MAX_H

#include <stdarg.h>
#include <stdbool.h>

#include <aul/exception.h>
#include <aul/string.h>
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

#define UNIXSOCK_ADDR	"unix:///var/local/maxkern.sock"


typedef void * (*malloc_f)(size_t size);
typedef void (*free_f)(void * ptr);
typedef void (*memerr_f)();

typedef struct
{
	exception_t * error;

	malloc_f malloc;
	free_f free;
	memerr_f memerr;

	hashtable_t syscalls;

	int sock;
	mutex_t sock_mutex;
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
	} data;
} return_t;


void max_init(maxhandle_t * hand);
void max_setmalloc(maxhandle_t * hand, malloc_f mfunc, free_f ffunc, memerr_f efunc);
void max_memerr();

bool max_connect(maxhandle_t * hand, const char * host);
#define max_connectlocal(hand) max_connect(hand, UNIXSOCK_ADDR)

string_t max_syscallsig(maxhandle_t * hand, const char * name);
bool max_syscallcache(maxhandle_t * hand, const char * name, const char * sig);
bool max_syscallexists(maxhandle_t * hand, const char * name, const char * sig);

return_t max_syscall(maxhandle_t * hand, const char * syscall, ...);
return_t max_vsyscall(maxhandle_t * hand, const char * syscall, va_list args);
return_t max_asyscall(maxhandle_t * hand, const char * syscall, void ** args);

bool max_iserror(maxhandle_t * hand);
const char * max_error(maxhandle_t * hand);
void max_clearerror(maxhandle_t * hand);

#ifdef __cplusplus
}
#endif
#endif
