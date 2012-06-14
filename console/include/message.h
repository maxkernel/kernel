#ifndef __MESSAGE_H
#define __MESSAGE_H

#include <stdarg.h>
#include <stdbool.h>

#include <aul/list.h>

#include <console.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
	P_FRAMING	= 0x00,
	P_HEADER,
	P_BODY,

	P_DONE		= 0x10,

	P_ERROR		= 0x20,
	P_EOF,
} msgstate_t;

typedef struct
{
	size_t headersize;
	char type;
	char * name;
	char * sig;

	size_t bodysize;
	void * body[CONSOLE_HEADERSIZE];
} message_t;

typedef struct
{
	list_t free_list;	// Maintain a list of free/used buffers. Used in console.c only

	size_t size;
	size_t index;
	char buffer[CONSOLE_BUFFERMAX];

	msgstate_t state;
	message_t msg;
} msgbuffer_t;



msgstate_t message_readfd(int fd, msgbuffer_t * buf);
bool message_writefd(int fd, char msgtype, const char * name, const char * sig, ...);
bool message_vwritefd(int fd, char msgtype, const char * name, const char * sig, va_list args);
bool message_awritefd(int fd, char msgtype, const char * name, const char * sig, void ** args);

message_t * message_getmessage(msgbuffer_t * buf);
msgstate_t message_getstate(msgbuffer_t * buf);

void message_reset(msgbuffer_t * buf);
#define message_clear(buf)		(memset((buf), 0, sizeof(msgbuffer_t)))

#ifdef __cplusplus
}
#endif
#endif
