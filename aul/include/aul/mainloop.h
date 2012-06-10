#ifndef __AUL_MAINLOOP_H
#define __AUL_MAINLOOP_H

#include <sys/epoll.h>

#include <aul/common.h>
#include <aul/mutex.h>
#include <aul/list.h>
#include <aul/hashtable.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUL_MAINLOOP_ROOT_NAME			"Root"
#define AUL_MAINLOOP_TIMEOUT_MS			10
#define AUL_MAINLOOP_MAX_WATCHERS		1500
#define AUL_MAINLOOP_MAX_TIMERS			1000

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
typedef bool (*timer_f)(mainloop_t * loop, uint64_t nanoseconds, void * userdata);

void mainloop_init();
mainloop_t * mainloop_new(const char * name);
void mainloop_run(mainloop_t * loop);
void mainloop_stop(mainloop_t * loop);

void mainloop_addwatch(mainloop_t * loop, int fd, fdcond_t cond, watch_f listener, void * userdata);
void mainloop_removewatch(mainloop_t * loop, int fd, fdcond_t cond);

void mainloop_addtimer(mainloop_t * loop, const char * name, uint64_t nanoseconds, timer_f listener, void * userdata);

#ifdef __cplusplus
}
#endif
#endif

