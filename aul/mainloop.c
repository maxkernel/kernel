#include <unistd.h>
#include <errno.h>
#include <sys/timerfd.h>

#include <aul/log.h>
#include <aul/mainloop.h>
#include <aul/stack.h>


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
	timerfd_f function;
	void * userdata;
} timerfdwatcher_t;

typedef struct
{
	list_t empty_list;

	const char * name;
	eventfd_f function;
	void * userdata;
} eventfdwatcher_t;

static watcher_t watchers[AUL_MAINLOOP_MAX_WATCHERS];
static timerfdwatcher_t timerfds[AUL_MAINLOOP_MAX_TIMERFDS];
static eventfdwatcher_t eventfds[AUL_MAINLOOP_MAX_EVENTFDS];

static stack_t empty_watchers;
static stack_t empty_timerfds;
static stack_t empty_eventfds;

static mutex_t mainloop_lock;
static mainloop_t * root = NULL;


static bool mainloop_timerfddispatch(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{
	timerfdwatcher_t * timerfdwatcher = userdata;

	uint64_t timeouts = 0;
	ssize_t r = read(fd, &timeouts, sizeof(timeouts));
	if (r != sizeof(uint64_t))
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Could not read all data from timerfd %s. Read %zd/%zu bytes: %s", timerfdwatcher->name, r, sizeof(uint64_t), strerror(errno));
		}

		return true;
	}
	else if (timeouts > 1)
	{
		log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Timerfd %s has been unsynchronized. It has fired %" PRIu64 " times without being dispatched!", timerfdwatcher->name, timeouts);
	}

	bool ret = timerfdwatcher->function(loop, timerfdwatcher->nanoseconds, timerfdwatcher->userdata);

	if (!ret)
	{
		// Listener returned false, remove the timerfd
		mutex_lock(&mainloop_lock);
		{
			stack_push(&empty_timerfds, &timerfdwatcher->empty_list);
		}
		mutex_unlock(&mainloop_lock);

		// Close the fd
		close(fd);
	}

	return ret;
}

static bool mainloop_eventfddispatch(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{
	eventfdwatcher_t * eventfdwatcher = userdata;

	eventfd_t counter = 0;
	if (eventfd_read(fd, &counter) < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Could not read all data from eventfd %s: %s", eventfdwatcher->name, strerror(errno));
		}

		return true;
	}

	bool ret = eventfdwatcher->function(loop, counter, eventfdwatcher->userdata);

	if (!ret)
	{
		// Listener returned false, remove the eventfd
		mutex_lock(&mainloop_lock);
		{
			stack_push(&empty_eventfds, &eventfdwatcher->empty_list);
		}
		mutex_unlock(&mainloop_lock);

		// Close the fd
		close(fd);
	}

	return ret;
}


bool mainloop_init(exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(root != NULL)
		{
			return true;
		}
	}

	mutex_init(&mainloop_lock, M_RECURSIVE);
	stack_init(&empty_watchers);
	stack_init(&empty_timerfds);
	stack_init(&empty_eventfds);

	for (size_t i = 0; i < AUL_MAINLOOP_MAX_WATCHERS; i++)
	{
		stack_push(&empty_watchers, &watchers[i].empty_list);
	}
	for (size_t i = 0; i < AUL_MAINLOOP_MAX_TIMERFDS; i++)
	{
		stack_push(&empty_timerfds, &timerfds[i].empty_list);
	}
	for (size_t i = 0; i < AUL_MAINLOOP_MAX_EVENTFDS; i++)
	{
		stack_push(&empty_eventfds, &eventfds[i].empty_list);
	}
	
	return (root = mainloop_new(AUL_MAINLOOP_ROOT_NAME, err)) != NULL;
}

mainloop_t * mainloop_new(const char * name, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(name == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}

	int efd = epoll_create(AUL_MAINLOOP_SIZE_HINT);
	// TODO - check for efd success!

	mainloop_t * loop = malloc(sizeof(mainloop_t));
	memset(loop, 0, sizeof(mainloop_t));

	loop->name = name;
	loop->epollfd = efd;
	loop->running = false;
	mutex_init(&loop->runlock, M_RECURSIVE);

	hashtable_init(&loop->watching, hash_int, hash_inteq);

	return loop;
}

bool mainloop_run(mainloop_t * loop, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(root == NULL)
		{
			exception_set(err, EINVAL, "Root mainloop has not been initialized");
			return false;
		}

		if (loop == NULL)
		{
			loop = root;
		}
	}


	volatile bool isrunning;
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
		if unlikely(!haswatchers)
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
						for (size_t i = 0; i < bits; i++)
						{
							watcher_t * watcher = events[i].data.ptr;
							if (!watcher->function(loop, watcher->fd, events[i].events, watcher->userdata))
							{
								mainloop_removefdwatch(loop, watcher->fd, events[i].events, NULL);
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

	return true;
}

bool mainloop_stop(mainloop_t * loop, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(root == NULL)
		{
			exception_set(err, EINVAL, "Root mainloop has not been initialized");
			return false;
		}

		if (loop == NULL)
		{
			loop = root;
		}
	}


	mutex_lock(&loop->runlock);
	{
		loop->running = false;
	}
	mutex_unlock(&loop->runlock);
	return true;
}

bool mainloop_addfdwatch(mainloop_t * loop, int fd, fdcond_t cond, watch_f listener, void * userdata, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(root == NULL)
		{
			exception_set(err, EINVAL, "Root mainloop has not been initialized");
			return false;
		}

		if (loop == NULL)
		{
			loop = root;
		}
	}

	watcher_t * watcher = NULL;

	// Retrieve a free watcher_t structure
	mutex_lock(&mainloop_lock);
	{
		list_t * entry = stack_pop(&empty_watchers);
		if (entry != NULL)
		{
			watcher = list_entry(entry, watcher_t, empty_list);
		}
	}
	mutex_unlock(&mainloop_lock);

	if (watcher == NULL)
	{
		// No free watcher_t, error
		exception_set(err, ENOMEM, "Could not add watch to mainloop. Out of free watchers! Consider increasing limit (AUL_MAINLOOP_MAX_WATCHERS = %d)", AUL_MAINLOOP_MAX_WATCHERS);
		return false;
	}

	// Initialize it and add it to the loops hashtable
	memset(watcher, 0, sizeof(watcher_t));
	watcher->fd = fd;
	watcher->function = listener;
	watcher->userdata = userdata;

	mutex_lock(&loop->runlock);
	{
		hashtable_put(&loop->watching, &watcher->fd, &watcher->watching_entry);
	}
	mutex_unlock(&loop->runlock);

	// Add it to epoll
	struct epoll_event event;
	memset(&event, 0, sizeof(struct epoll_event));
	event.events = cond;
	event.data.ptr = watcher;

	int success = epoll_ctl(loop->epollfd, EPOLL_CTL_ADD, fd, &event);
	if (success < 0)
	{
		exception_set(err, errno, "Could not configure epoll to watch fd: %s", strerror(errno));

		// Remove watcher from hashtable
		mutex_lock(&loop->runlock);
		{
			hashtable_remove(&watcher->watching_entry);
		}
		mutex_unlock(&loop->runlock);

		// Add watcher back to empty pool
		mutex_lock(&mainloop_lock);
		{
			stack_push(&empty_watchers, &watcher->empty_list);
		}
		mutex_unlock(&mainloop_lock);

		return false;
	}

	return true;
}

bool mainloop_removefdwatch(mainloop_t * loop, int fd, fdcond_t cond, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(root == NULL)
		{
			exception_set(err, EINVAL, "Root mainloop has not been initialized");
			return false;
		}

		if (loop == NULL)
		{
			loop = root;
		}
	}


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
		exception_set(err, EINVAL, "Mainloop %s not currently watching fd %d", loop->name, fd);
		return false;
	}

	// Add the watcher to the empty list
	mutex_lock(&mainloop_lock);
	{
		stack_push(&empty_watchers, &watcher->empty_list);
	}
	mutex_unlock(&mainloop_lock);

	// Remove the watcher
	struct epoll_event event;
	memset(&event, 0, sizeof(struct epoll_event));
	event.events = cond;

	// Remove the watcher from epoll
	int success = epoll_ctl(loop->epollfd, EPOLL_CTL_DEL, fd, &event);
	if (success < 0)
	{
		exception_set(err, errno, "Could not configure epoll to unwatch fd %d: %s", fd, strerror(errno));
		return false;
	}

	return true;
}

int mainloop_addnewfdtimer(mainloop_t * loop, const char * name, uint64_t nanoseconds, timerfd_f listener, void * userdata, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(root == NULL)
		{
			exception_set(err, EINVAL, "Root mainloop has not been initialized");
			return false;
		}

		if (loop == NULL)
		{
			loop = root;
		}
	}


	// Retrieve a free timerfdwatcher_t structure
	timerfdwatcher_t * timerfdwatcher = NULL;
	mutex_lock(&mainloop_lock);
	{
		list_t * entry = stack_pop(&empty_timerfds);
		if (entry != NULL)
		{
			timerfdwatcher = list_entry(entry, timerfdwatcher_t, empty_list);
		}
	}
	mutex_unlock(&mainloop_lock);

	if (timerfdwatcher == NULL)
	{
		// No free watcher_t, error
		exception_set(err, ENOMEM, "Could not add timerfd to mainloop. Out of free timerfds! Consider increasing limit (AUL_MAINLOOP_MAX_TIMERFDS = %d)", AUL_MAINLOOP_MAX_TIMERFDS);
		return false;
	}

	// Put data into timerfdwatcher_t structure
	memset(timerfdwatcher, 0, sizeof(timerfdwatcher_t));
	timerfdwatcher->name = name;
	timerfdwatcher->nanoseconds = nanoseconds;
	timerfdwatcher->function = listener;
	timerfdwatcher->userdata = userdata;

	// Get the current time
	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
	{
		exception_set(err, EFAULT, "Could not get system time to create timerfd %s: %s", name, strerror(errno));
		return false;
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
	int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if (fd == -1)
	{
		exception_set(err, EFAULT, "Could not create timerfd %s: %s", name, strerror(errno));
		return false;
	}

	if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &timer, NULL) == -1)
	{
		exception_set(err, EFAULT, "Could not set timerfd %s: %s", name, strerror(errno));
		return false;
	}

	if (!mainloop_addfdwatch(loop, fd, EPOLLET | FD_READ, mainloop_timerfddispatch, timerfdwatcher, err))
	{
		// Release watcher back to empty list
		mutex_lock(&mainloop_lock);
		{
			stack_push(&empty_timerfds, &timerfdwatcher->empty_list);
		}
		mutex_unlock(&mainloop_lock);
		close(fd);

		return -1;
	}

	return fd;
}

int mainloop_addnewfdevent(mainloop_t * loop, const char * name, unsigned int initialvalue, eventfd_f listener, void * userdata, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return -1;
		}

		if unlikely(root == NULL)
		{
			exception_set(err, EINVAL, "Root mainloop has not been initialized");
			return -1;
		}

		if (loop == NULL)
		{
			loop = root;
		}
	}

	// Retrieve a free eventfdwatcher_t structure
	eventfdwatcher_t * eventfdwatcher = NULL;
	mutex_lock(&mainloop_lock);
	{
		list_t * entry = stack_pop(&empty_eventfds);
		if (entry != NULL)
		{
			eventfdwatcher = list_entry(entry, eventfdwatcher_t, empty_list);
		}
	}
	mutex_unlock(&mainloop_lock);

	if (eventfdwatcher == NULL)
	{
		// No free watcher_t, error
		exception_set(err, ENOMEM, "Could not add eventfd to mainloop. Out of free eventfds! Consider increasing limit (AUL_MAINLOOP_MAX_EVENTFDS = %d)", AUL_MAINLOOP_MAX_EVENTFDS);
		return -1;
	}

	// Put data into eventfdwatcher_t structure
	memset(eventfdwatcher, 0, sizeof(eventfdwatcher_t));
	eventfdwatcher->name = name;
	eventfdwatcher->function = listener;
	eventfdwatcher->userdata = userdata;

	// Create the file descriptor
	int fd = eventfd(initialvalue, EFD_NONBLOCK);
	if (fd == -1)
	{
		exception_set(err, EFAULT, "Could not create eventfd %s: %s", name, strerror(errno));
		return -1;
	}

	if (!mainloop_addfdwatch(loop, fd, EPOLLET | FD_READ, mainloop_eventfddispatch, eventfdwatcher, err))
	{
		// Release watcher back to empty list
		mutex_lock(&mainloop_lock);
		{
			stack_push(&empty_eventfds, &eventfdwatcher->empty_list);
		}
		mutex_unlock(&mainloop_lock);
		close(fd);

		return -1;
	}

	return fd;
}
