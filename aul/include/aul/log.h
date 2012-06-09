#ifndef __AUL_LOG_H
#define __AUL_LOG_H

#include <stdarg.h>
#include <aul/common.h>
#include <aul/exception.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUL_LOG_DOMAIN			"A-UTIL"
#define AUL_LOG_MAXLISTENERS	10
#define AUL_LOG_MAXFILESIZE		(400 * 1024)	// 400 KB

typedef enum
{
	LEVEL_FATAL		= (1 << 0),
	LEVEL_ERROR		= (1 << 1),
	LEVEL_WARNING	= (1 << 2),
	LEVEL_INFO		= (1 << 3),
	LEVEL_DEBUG		= (1 << 4),
} level_t;

typedef void (*log_f)(level_t level, const char * domain, uint64_t milliseconds, const char * message, void * userdata);
typedef void (*logclose_f)(void * userdata);

void log_destroy();
bool log_addfd(int fd, exception_t ** err);
bool log_openfile(const char * path, exception_t ** err);
void log_dispatch(level_t level, const char * domain, const char * fmt, va_list args);
void log_addlistener(log_f listener, logclose_f closer, void * userdata);
void log_removelistener(log_f listener);
void log_setdefault(log_f listener, logclose_f closer, void * userdata);

static inline check_printf(3, 4) void log_write(level_t level, const char * domain, const char * fmt, ...)
{
#if !defined(ALPHA) && !defined(BETA)
	if (level == LEVEL_DEBUG)
	{
		return;
	}
#endif

	va_list args;
	va_start(args, fmt);
	log_dispatch(level, domain, fmt, args);
	va_end(args);
}

#ifdef __cplusplus
}
#endif
#endif
