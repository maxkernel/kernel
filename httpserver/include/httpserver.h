#ifndef __HTTPSERVER_H
#define __HTTPSERVER_H

#include <inttypes.h>
#include <stdarg.h>

#include <aul/common.h>
#include <aul/mainloop.h>
#include <aul/hashtable.h>
#include <aul/exception.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __http_context http_context;
typedef int http_connection;

typedef void (*http_callback)(http_connection * conn, http_context * ctx, const char * uri);

typedef enum
{
	MATCH_ALL,
	MATCH_PREFIX
} http_match;

const char * http_getheader(http_context * ctx, const char * name);
const char * http_getparam(http_context * ctx, const char * name);

http_context * http_new(uint16_t port, mainloop_t * mainloop, exception_t ** error);
void http_adduri(http_context * ctx, const char * uri, http_match match, http_callback cb, void * userdata);

void http_printf(http_connection * conn, const char * fmt, ...) CHECK_PRINTF(2, 3);
void http_vprintf(http_connection * conn, const char * fmt, va_list args);
void http_write(http_connection * conn, const void * buf, size_t len);

void http_destroy(http_context * ctx);

// Helper functions
void http_urldecode(char * string);

#ifdef __cplusplus
}
#endif
#endif
