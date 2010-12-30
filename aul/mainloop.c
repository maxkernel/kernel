#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/timerfd.h>

#include <aul/log.h>
#include <aul/common.h>
#include <aul/mainloop.h>

struct __timer
{
	const char * name;
	timer_f function;
	void * userdata;
};

struct __listener
{
	watch_f function;
	void * userdata;
};

struct __watcher
{
	int fd;
	struct __listener readlistener;
	struct __listener writelistener;
	struct __listener errorlistener;
	list_t list;
} watchers[FD_SETSIZE];

static mainloop_t * root = NULL;

void mainloop_init()
{
	if (root != NULL)
	{
		return;
	}

	size_t i;

	memset(watchers, 0, sizeof(watchers));
	for (i=0; i<FD_SETSIZE; i++)
	{
		watchers[i].fd = -1;
	}

	root = mainloop_new("Root");
}

mainloop_t * mainloop_new(const char * name)
{
	mainloop_t * loop = malloc(sizeof(mainloop_t));
	memset(loop, 0, sizeof(mainloop_t));
	
	loop->name = name;
	loop->timeout.tv_sec = AUL_MAINLOOP_TIMEOUT_US / 1000000;
	loop->timeout.tv_usec = AUL_MAINLOOP_TIMEOUT_US % 1000000;
	mutex_init(&loop->runlock, M_RECURSIVE);
	LIST_INIT(&loop->list);
	FD_ZERO(&loop->readset);
	FD_ZERO(&loop->writeset);
	FD_ZERO(&loop->errorset);
	
	return loop;
}

void mainloop_run(mainloop_t * loop)
{
	if (root == NULL)
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "MainLoop subsystem not been initialized!");
		return;
	}
	if (loop == NULL)	loop = root;


	String loopname;
	bool isruning;
	int maxfd, numwatches;
	fd_set readset, writeset, errorset;
	struct timeval timeout;

	ZERO(loopname);

	mutex_lock(&loop->runlock);
	loop->running = true;
	string_append(&loopname, "%s", loop->name);
	mutex_unlock(&loop->runlock);


	do
	{
		mutex_lock(&loop->runlock);

		memcpy(&readset, &loop->readset, sizeof(fd_set));
		memcpy(&writeset, &loop->writeset, sizeof(fd_set));
		memcpy(&errorset, &loop->errorset, sizeof(fd_set));
		memcpy(&timeout, &loop->timeout, sizeof(struct timeval));

		isruning = loop->running;
		maxfd = loop->maxfd;
		numwatches = loop->numwatches;

		mutex_unlock(&loop->runlock);

		if (isruning)
		{
			int bits = 0;
			if (numwatches == 0)
			{
				usleep(AUL_MAINLOOP_TIMEOUT_US);
			}
			else
			{
				bits = select(maxfd+1, &readset, &writeset, &errorset, &timeout);
			}

			if (bits == -1)
			{
				if (errno != EINTR)
				{
					log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Mainloop (%s): %s", loopname.string, strerror(errno));
				}
			}
			else if (bits > 0)
			{
				mutex_lock(&loop->runlock);

				if (loop->running)
				{
					list_t * pos, * q;
					list_foreach_safe(pos, q, &loop->list)
					{
						struct __watcher * item = list_entry(pos, struct __watcher, list);

#define __mainloop__dispatchmacro(type, lname, fdset)											\
						if (item->lname.function != NULL && FD_ISSET(item->fd, &fdset))			\
						{																		\
							if (!item->lname.function(item->fd, type, item->lname.userdata))	\
							{																	\
								if (item->fd != -1)												\
									mainloop_removewatch(loop, item->fd, type);					\
							}																	\
						}																		\

						__mainloop__dispatchmacro(FD_READ, readlistener, readset)
						__mainloop__dispatchmacro(FD_WRITE, writelistener, writeset)
						__mainloop__dispatchmacro(FD_ERROR, errorlistener, errorset)
					}
				}

				mutex_unlock(&loop->runlock);
			}
		}
	} while (isruning);
}

void mainloop_stop(mainloop_t * loop)
{
	if (root == NULL)
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "MainLoop subsystem not been initialized!");
		return;
	}
	if (loop == NULL)	loop = root;

	mutex_lock(&loop->runlock);
	loop->running = false;
	mutex_unlock(&loop->runlock);
}

void mainloop_addwatch(mainloop_t * loop, int fd, fdcond_t cond, watch_f listener, void * userdata)
{
	if (root == NULL)
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "MainLoop subsystem not been initialized!");
		return;
	}
	if (loop == NULL)	loop = root;

	if (watchers[fd].fd != -1)
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Another mainloop already already watching file desciptor %d", fd);
		return;
	}


	mutex_lock(&loop->runlock);

	if (fd > loop->maxfd)
	{
		loop->maxfd = fd;
	}

#define __mainloop__addmacro(type, lname, fdset)		\
	if ((cond & type) != 0)								\
	{													\
		if (watchers[fd].lname.function == NULL)		\
		{												\
			loop->numwatches += 1;						\
			FD_SET(fd, &loop->fdset);					\
			watchers[fd].fd = fd;						\
			watchers[fd].lname.function = listener;		\
			watchers[fd].lname.userdata = userdata;		\
		}												\
		else											\
		{												\
			log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Watcher already added to file desciptor %d in mainloop %s", fd, loop->name); \
		}												\
	}													\

	__mainloop__addmacro(FD_READ, readlistener, readset)
	__mainloop__addmacro(FD_WRITE, writelistener, writeset)
	__mainloop__addmacro(FD_ERROR, errorlistener, errorset)

	list_add(&loop->list, &watchers[fd].list);
	mutex_unlock(&loop->runlock);
}

void mainloop_removewatch(mainloop_t * loop, int fd, fdcond_t cond)
{
	if (root == NULL)
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "MainLoop subsystem not been initialized!");
		return;
	}
	if (loop == NULL)	loop = root;

	mutex_lock(&loop->runlock);

#define __mainloop__delmacro(type, lname, fdset)		\
	if ((cond & type) != 0)								\
	{													\
		if (watchers[fd].lname.function != NULL)		\
		{												\
			loop->numwatches -= 1;						\
			FD_CLR(fd, &loop->fdset);					\
			watchers[fd].lname.function = NULL;			\
		}												\
		else											\
		{												\
			log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Watcher does not exist on file descriptor %d in mainloop %s", fd, loop->name); \
		}												\
	}													\


	__mainloop__delmacro(FD_READ, readlistener, readset)
	__mainloop__delmacro(FD_WRITE, writelistener, writeset)
	__mainloop__delmacro(FD_ERROR, errorlistener, errorset)

	if (watchers[fd].readlistener.function == NULL && watchers[fd].writelistener.function == NULL && watchers[fd].errorlistener.function == NULL)
	{
		watchers[fd].fd = -1;
		list_remove(&watchers[fd].list);
	}

	mutex_unlock(&loop->runlock);
}



static bool mainloop_timerdispatch(int fd, fdcond_t cond, void * userdata)
{
	struct __timer * tm = userdata;

	uint64_t timeouts = 0;
	read(fd, &timeouts, sizeof(timeouts));

	if (timeouts > 1)
	{
		log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Timer %s has been unsynchronized. It has fired %" PRIu64 " times without being dispatched!", tm->name, timeouts);
	}


	return tm->function(tm->userdata);
}

void mainloop_addtimer(mainloop_t * loop, const char * name, uint64_t nanoseconds, timer_f listener, void * userdata)
{
	struct timespec now;
	if (clock_gettime(CLOCK_REALTIME, &now) == -1)
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Could not get system time to create timer %s: %s", name, strerror(errno));
		return;
	}

	struct itimerspec timer;
	timer.it_interval.tv_nsec = nanoseconds % AUL_NANOS_PER_SEC;
	timer.it_interval.tv_sec = nanoseconds / AUL_NANOS_PER_SEC;
	timer.it_value.tv_nsec = now.tv_nsec + timer.it_interval.tv_nsec;
	timer.it_value.tv_sec = now.tv_sec + timer.it_interval.tv_sec;

	if (timer.it_value.tv_nsec > NANOS_PER_SECOND)
	{
		timer.it_value.tv_nsec -= NANOS_PER_SECOND;
		timer.it_value.tv_sec += 1;
	}

	int fd = timerfd_create(CLOCK_REALTIME, 0);
	if (fd == -1)
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Could not create timer %s: %s", name, strerror(errno));
		return;
	}

	if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &timer, NULL) == -1)
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Could not set timer %s: %s", name, strerror(errno));
		return;
	}

	struct __timer * tm = malloc(sizeof(struct __timer));
	tm->name = name;
	tm->function = listener;
	tm->userdata = userdata;

	mainloop_addwatch(loop, fd, FD_READ, mainloop_timerdispatch, tm);
}
