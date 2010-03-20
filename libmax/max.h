#ifndef __MAX_H
#define __MAX_H

#include <stdarg.h>

#include <buffer.h>
#include <array.h>

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
#define T_ARRAY_BOOLEAN	'B'
#define T_ARRAY_INTEGER	'I'
#define T_ARRAY_DOUBLE	'D'
#define T_BUFFER		'x'

typedef struct {
	//private data
	void * gerror;

	int sock_type;
	void * sock;
	void * sock_io;
} maxhandle_t;

maxhandle_t * max_local();
maxhandle_t * max_remote(const char * host);
void max_close(maxhandle_t * hand);

int max_syscall(maxhandle_t * hand, const char * syscall, const char * signature, void * return_value, ...);
int max_vsyscall(maxhandle_t * hand, const char * syscall, const char * signature, void * return_value, va_list args);
int max_asyscall(maxhandle_t * hand, const char * syscall, const char * signature, void * return_value, void ** args);

int max_iserror(maxhandle_t * hand);
const char * max_error(maxhandle_t * hand);
void max_clearerror(maxhandle_t * hand);

#ifdef __cplusplus
}
#endif
#endif
