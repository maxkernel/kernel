#include <unistd.h>
#include <errno.h>
#include <sys/timerfd.h>

#include <aul/log.h>
#include <aul/mainloop.h>


typedef struct
{
	list_t empty_list;
	hashentry_t watching_entry;
	
	int fd;
	watch_f function;
	void * userdata;
} watcher_t;

typedef struct
{
	list_t empty_list;

	const char * name;
	uint64_t nanoseconds;
	timer_f function;
	void * userdata;
} timerwatcher_t;

static watcher_t watchers[AUL_MAINLOOP_MAX_WATCHERS];
static timerwatcher_t timers[AUL_MAINLOOP_MAX_TIMERS];

static list_t empty_watchers;
static list_t empty_timers;

static mutex_t mainloop_lock;
static mainloop_t * root = NULL;


#define check_init() do { \
	if (root == NULL) { log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Mainloop subsystem not been initialized!"); return; } \
} while (0)

#define resolve_loop() do { \
	if (loop == NULL) { loop = root; } \
} while (0)


static bool mainloop_timerdispatch(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{
	timerwatcher_t * timerwatcher = userdata;

	uint64_t timeouts = 0;
	read(fd, &timeouts, sizeof(timeouts));

	if (timeouts > 1)
	{
		log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Timer %s has been unsynchronized. It has fired %" PRIu64 " times without being dispatched!", timerwatcher->name, timeouts);
	}

	bool ret = timerwatcher->function(loop, timerwatcher->nanoseconds, timerwatcher->userdata);

	if (!ret)
	{
		// Listener returned false, remove the timer
		mutex_lock(&mainloop_lock);
		{
			list_add(&empty_timers, &timerwatcher->empty_list);
		}
		mutex_unlock(&mainloop_lock);
	}

	return ret;
}


void mainloop_init()
{
	// Sanity check
	if (root != NULL)
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Mainloop subsystem has already been initialized!");
		return;
	}

	mutex_init(&mainloop_lock, M_RECURSIVE);
	LIST_INIT(&empty_watchers);
	LIST_INIT(&empty_timers);

	size_t i;
	for (i=0; i<AUL_MAINLOOP_MAX_WATCHERS; i++)
	{
		list_add(&empty_watchers, &watchers[i].empty_list);
	}
	for (i=0; i<AUL_MAINLOOP_MAX_TIMERS; i++)
	{
		list_add(&empty_timers, &timers[i].empty_list);
	}
	
	root = malloc0(sizeof(mainloop_t));
	mainloop_new(AUL_MAINLOOP_ROOT_NAME, root);
}

void mainloop_new(const char * name, mainloop_t * loop)
{
	PZERO(loop, sizeof(mainloop_t));

	loop->name = name;
	loop->epollfd = epoll_create(AUL_MAINLOOP_SIZE_HINT);
	loop->running = false;
	mutex_init(&loop->runlock, M_RECURSIVE);

	HASHTABLE_INIT(&loop->watching, hash_int, hash_inteq);
}

void mainloop_run(mainloop_t * loop)
{
	check_init();
	resolve_loop();

	bool isrunning;
	bool haswatchers;
	struct epoll_event events[AUL_MAINLOOP_EPOLL_EVENTS];

	// Set our running status
	mutex_lock(&loop->runlock);
	{
		isrunning = loop->running = true;
		haswatchers = !hashtable_isempty(&loop->watching);
	}
	mutex_unlock(&loop->runlock);

	do
	{

		if (UNLIKELY( !haswatchers ))
		{
			// We are not watching any descriptors, just sleep
			usleep(AUL_MAINLOOP_TIMEOUT_MS * 1000);
		}
		else
		{
			int bits = epoll_wait(loop->epollfd, events, AUL_MAINLOOP_EPOLL_EVENTS, AUL_MAINLOOP_TIMEOUT_MS);

			if (bits == -1 && errno != EINTR)
			{
				log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Mainloop (%s): %s", loop->name, strerror(errno));
			}
			else if (bits > 0)
			{
				mutex_lock(&loop->runlock);
				{
					if (&loop->running)
					{
						int i = 0;
						for (; i<bits; i++)
						{
							watcher_t * watcher = events[i].data.ptr;
							if (!watcher->function(loop, watcher->fd, events[i].events, watcher->userdata))
							{
								mainloop_removewatch(loop, watcher->fd, events[i].events);
							}
						}
					}
				}
				mutex_unlock(&loop->runlock);
			}
		}

		// Update up running status
		mutex_lock(&loop->runlock);
		{
			isrunning = loop->running;
			haswatchers = !hashtable_isempty(&loop->watching);
		}
		mutex_unlock(&loop->runlock);

	} while (isrunning);
}

void mainloop_stop(mainloop_t * loop)
{
	check_init();
	resolve_loop();

	mutex_lock(&loop->runlock);
	{
		loop->running = false;
	}
	mutex_unlock(&loop->runlock);
}

void mainloop_addwatch(mainloop_t * loop, int fd, fdcond_t cond, watch_f listener, void * userdata)
{
	check_init();
	resolve_loop();

	watcher_t * watcher = NULL;
	struct epoll_event event;

	// Retrieve a free watcher_t structure
	mutex_lock(&mainloop_lock);
	{
		if (!list_isempty(&empty_watchers))
		{
			watcher = list_entry(empty_watchers.next, watcher_t, empty_list);
			list_remove(&watcher->empty_list);
		}
	}
	mutex_unlock(&mainloop_lock);

	if (watcher == NULL)
	{
		// No free watcher_t, error
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Could not add watch to mainloop. Out of free watchers! Consider increasing limit (currently %d)", AUL_MAINLOOP_MAX_WATCHERS);
		return;
	}

	// Initialize it and add it to the loops hashtable
	PZERO(watcher, sizeof(watcher_t));
	watcher->fd = fd;
	watcher->function = listener;
	watcher->userdata = userdata;

	mutex_lock(&loop->runlock);
	{
		hashtable_put(&loop->watching, &watcher->fd, &watcher->watching_entry);
	}
	mutex_unlock(&loop->runlock);

	// Add it to epoll
	ZERO(event);
	event.events = cond;
	event.data.ptr = watcher;

	epoll_ctl(loop->epollfd, EPOLL_CTL_ADD, fd, &event);
}

void mainloop_addtimer(mainloop_t * loop, const char * name, uint64_t nanoseconds, timer_f listener, void * userdata)
{
	check_init();
	resolve_loop();


	// Retrieve a free timerwatcher_t structure
	timerwatcher_t * timerwatcher = NULL;
	mutex_lock(&mainloop_lock);
	{
		if (!list_isempty(&empty_timers))
		{
			timerwatcher = list_entry(empty_timers.next, timerwatcher_t, empty_list);
			list_remove(&timerwatcher->empty_list);
		}
	}
	mutex_unlock(&mainloop_lock);

	if (timerwatcher == NULL)
	{
		// No free watcher_t, error
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Could not add timer to mainloop. Out of free timers! Consider increasing limit (currently %d)", AUL_MAINLOOP_MAX_TIMERS);
		return;
	}

	// Put data into timerwatcher_t structure
	PZERO(timerwatcher, sizeof(timerwatcher_t));
	timerwatcher->name = name;
	timerwatcher->nanoseconds = nanoseconds;
	timerwatcher->function = listener;
	timerwatcher->userdata = userdata;

	// Get the current time
	struct timespec now;
	if (clock_gettime(CLOCK_REALTIME, &now) == -1)
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Could not get system time to create timer %s: %s", name, strerror(errno));
		return;
	}

	// Set up the timer
	struct itimerspec timer;
	timer.it_interval.tv_nsec = nanoseconds % NANOS_PER_SECOND;
	timer.it_interval.tv_sec = nanoseconds / NANOS_PER_SECOND;
	timer.it_value.tv_nsec = now.tv_nsec + timer.it_interval.tv_nsec;
	timer.it_value.tv_sec = now.tv_sec + timer.it_interval.tv_sec;

	if (timer.it_value.tv_nsec > NANOS_PER_SECOND)
	{
		timer.it_value.tv_nsec -= NANOS_PER_SECOND;
		timer.it_value.tv_sec += 1;
	}

	// Create the file descriptor
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

	mainloop_addwatch(loop, fd, FD_READ, mainloop_timerdispatch, timerwatcher);
}

void mainloop_removewatch(mainloop_t * loop, int fd, fdcond_t cond)
{
	check_init();
	resolve_loop();

	// Get the watcher from the hashtable
	watcher_t * watcher = NULL;
	mutex_lock(&loop->runlock);
	{
		hashentry_t * entry = hashtable_get(&loop->watching, &fd);
		if (entry != NULL)
		{
			watcher = hashtable_entry(entry, watcher_t, watching_entry);
			hashtable_remove(entry);
		}
	}
	mutex_unlock(&loop->runlock);

	if (watcher == NULL)
	{
		log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Could not remove fd %d from mainloop, not presently watching it!", fd);
		return;
	}


	// Remove the watcher from epoll
	epoll_ctl(loop->epollfd, EPOLL_CTL_DEL, cond, NULL);

	// Add the watcher to the empty list
	mutex_lock(&mainloop_lock);
	{
		list_add(&empty_watchers, &watcher->empty_list);
	}
	mutex_unlock(&mainloop_lock);
}
