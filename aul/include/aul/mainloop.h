#ifndef __AUL_MAINLOOP_H
#define __AUL_MAINLOOP_H

#include <sys/epoll.h>

#include <aul/common.h>
#include <aul/exception.h>
#include <aul/mutex.h>
#include <aul/list.h>
#include <aul/hashtable.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUL_MAINLOOP_ROOT_NAME			"Root"
#define AUL_MAINLOOP_TIMEOUT_MS			10
#define AUL_MAINLOOP_MAX_WATCHERS		300
#define AUL_MAINLOOP_MAX_TIMERFDS			100
#define AUL_MAINLOOP_MAX_EVENTFDS			100

#define AUL_MAINLOOP_SIZE_HINT			10
#define AUL_MAINLOOP_EPOLL_EVENTS		100

typedef enum
{
	FD_NONE		= 0,
	FD_READ		= EPOLLIN,
	FD_READ_ET	= (EPOLLET | EPOLLRDHUP),
	FD_WRITE	= EPOLLOUT,
	FD_HANGUP	= EPOLLRDHUP,
	FD_ERROR	= EPOLLERR,
} fdcond_t;

typedef struct
{
	const char * name;
	int epollfd;
	volatile bool running;
	mutex_t runlock;
	
	hashtable_t watching;
} mainloop_t;

typedef bool (*watch_f)(mainloop_t * loop, int fd, fdcond_t condition, void * userdata);
typedef bool (*timerfd_f)(mainloop_t * loop, uint64_t nanoseconds, void * userdata);
typedef bool (*eventfd_f)(mainloop_t * loop, uint64_t counter, void * userdata);

bool mainloop_init(exception_t ** err);
mainloop_t * mainloop_new(const char * name, exception_t ** err);
bool mainloop_run(mainloop_t * loop, exception_t ** err);
bool mainloop_stop(mainloop_t * loop, exception_t ** err);

bool mainloop_addwatch(mainloop_t * loop, int fd, fdcond_t cond, watch_f listener, void * userdata, exception_t ** err);
bool mainloop_removewatch(mainloop_t * loop, int fd, fdcond_t cond, exception_t ** err);

bool mainloop_newtimerfd(mainloop_t * loop, const char * name, uint64_t nanoseconds, timerfd_f listener, void * userdata, exception_t ** err);
int mainloop_neweventfd(mainloop_t * loop, const char * name, unsigned int initialvalue, eventfd_f listener, void * userdata, exception_t ** err);

#ifdef __cplusplus
}
#endif
#endif

