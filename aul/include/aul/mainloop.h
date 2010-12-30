#ifndef __AUL_MAINLOOP_H
#define __AUL_MAINLOOP_H

#include <inttypes.h>

#include <aul/common.h>
#include <aul/mutex.h>

#include <aul/contrib/list.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUL_MAINLOOP_TIMEOUT_US		10000
#define AUL_MAINLOOP_SLEEP_US		100
#define AUL_NANOS_PER_SEC		1000000000LL

typedef enum
{
	FD_NONE		= 0,
	FD_READ		= 1 << 1,
	FD_WRITE	= 1 << 2,
	FD_ERROR	= 1 << 3
} fdcond_t;

typedef bool (*watch_f)(int fd, fdcond_t condition, void * userdata);
typedef bool (*timer_f)(void * userdata);

struct __watcher;

typedef struct
{
	const char * name;
	volatile bool running;
	mutex_t runlock;
	
	list_t list;
	int numwatches, maxfd;

	fd_set readset, writeset, errorset;
	struct timeval timeout;
} mainloop_t;

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
