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

typedef struct __httpcontext_t httpcontext_t;
typedef int httpconnection_t;

typedef void (*httpcallback_f)(httpconnection_t * conn, httpcontext_t * ctx, const char * uri);

typedef enum
{
	MATCH_ALL,
	MATCH_PREFIX
} httpmatch_t;

const char * http_getheader(httpcontext_t * ctx, const char * name);
const char * http_getparam(httpcontext_t * ctx, const char * name);

httpcontext_t * http_new(uint16_t port, mainloop_t * mainloop, exception_t ** err);
void http_adduri(httpcontext_t * ctx, const char * uri, httpmatch_t match, httpcallback_f cb, void * userdata);

void http_printf(httpconnection_t * conn, const char * fmt, ...) CHECK_PRINTF(2, 3);
void http_vprintf(httpconnection_t * conn, const char * fmt, va_list args);
void http_write(httpconnection_t * conn, const void * buf, size_t len);

void http_destroy(httpcontext_t * ctx);

// Helper functions
void http_urldecode(char * string);

#ifdef __cplusplus
}
#endif
#endif
