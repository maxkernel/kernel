#ifndef __AUL_MAINLOOP_H
#define __AUL_MAINLOOP_H

#include <sys/epoll.h>
#include <sys/eventfd.h>

#include <aul/common.h>
#include <aul/exception.h>
#include <aul/mutex.h>
#include <aul/list.h>
#include <aul/hashtable.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUL_MAINLOOP_TIMEOUT_MS			100

#define AUL_MAINLOOP_SIZE_HINT			50
#define AUL_MAINLOOP_EPOLL_EVENTS		100

typedef enum
{
	FD_EDGE_TRIG	= EPOLLET,
	FD_READ			= EPOLLIN,
	FD_WRITE		= EPOLLOUT,
} fdcond_t;

typedef struct
{
	const char * name;
	int epollfd;
	volatile bool running;
} mainloop_t;

typedef bool (*watch_f)(mainloop_t * loop, int fd, fdcond_t condition, void * userdata);
typedef bool (*timerfd_f)(mainloop_t * loop, uint64_t nanoseconds, void * userdata);
typedef bool (*eventfd_f)(mainloop_t * loop, eventfd_t counter, void * userdata);

typedef struct
{
	mainloop_t * loop;

	int fd;
	uint32_t events;
	watch_f listener;
	void * userdata;
} fdwatcher_t;

typedef struct
{
	fdwatcher_t watcher;

	const char * name;		// TODO - Is this necessary?
	uint64_t nanoseconds;
	timerfd_f listener;
	void * userdata;
} timerwatcher_t;

typedef struct
{
	fdwatcher_t watcher;

	const char * name;		// TODO - Is this necessary?
	eventfd_f listener;
	void * userdata;
} eventwatcher_t;

mainloop_t * mainloop_new(const char * name, exception_t ** err);
bool mainloop_run(mainloop_t * loop, exception_t ** err);
bool mainloop_stop(mainloop_t * loop, exception_t ** err);

bool mainloop_addwatcher(mainloop_t * loop, fdwatcher_t * watcher, exception_t ** err);
bool mainloop_removewatcher(fdwatcher_t * watcher, exception_t ** err);

void watcher_newfd(fdwatcher_t * watcher, int fd, fdcond_t events, watch_f listener, void * userdata);
bool watcher_newtimer(timerwatcher_t * timerwatcher, const char * name, uint64_t nanoseconds, timerfd_f listener, void * userdata, exception_t ** err);
bool watcher_newevent(eventwatcher_t * eventwatcher, const char * name, unsigned int initialvalue, eventfd_f listener, void * userdata, exception_t ** err);
#define watcher_fd(w)		((w)->fd)
#define watcher_events(w)	((w)->events)
#define watcher_data(w)		((w)->userdata)

#define watcher_cast(o)		(&(o)->watcher)
#define watcher_init(w)		({ (w)->loop = NULL; (w)->fd = -1; (w)->events = 0; (w)->listener = NULL; (w)->userdata = NULL; })
#define watcher_close(w)	({ close(watcher_fd(w)); watcher_init(w); })

#ifdef __cplusplus
}
#endif
#endif

