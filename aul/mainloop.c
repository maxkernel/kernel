#include <unistd.h>
#include <errno.h>
#include <sys/timerfd.h>

#include <aul/log.h>
#include <aul/mainloop.h>


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
	if (efd < 0)
	{
		exception_set(err, errno, "Mainloop (%s) creation failure: %s", name, strerror(errno));
		return NULL;
	}

	mainloop_t * loop = malloc(sizeof(mainloop_t));
	memset(loop, 0, sizeof(mainloop_t));
	loop->name = name;
	loop->epollfd = efd;
	loop->running = false;

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

		if unlikely(loop == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}
	}

	loop->running = true;
	struct epoll_event events[AUL_MAINLOOP_EPOLL_EVENTS];

	while (loop->running)
	{
		/*
		if unlikely(!haswatchers)
		{
			// We are not watching any descriptors, just sleep
			usleep(AUL_MAINLOOP_TIMEOUT_MS * 1000);
		}
		else
		*/
		{
			int bits = epoll_wait(loop->epollfd, events, AUL_MAINLOOP_EPOLL_EVENTS, AUL_MAINLOOP_TIMEOUT_MS);
			if unlikely(bits == -1 && errno != EINTR)
			{
				log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Mainloop (%s): %s", loop->name, strerror(errno));
			}
			else if likely(bits > 0)
			{
				for (size_t i = 0; i < bits; i++)
				{
					fdwatcher_t * watcher = events[i].data.ptr;
					//log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "H %d", watcher->fd);

					if (!watcher->listener(loop, watcher->fd, events[i].events, watcher->userdata))
					{
						exception_t * e = NULL;
						if (!mainloop_removewatcher(watcher, &e) || exception_check(&e))
						{
							log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Could not remove watch from mainloop (%s) fd %d: %s", loop->name, watcher->fd, exception_message(e));
							exception_free(e);
						}
					}
				}
			}
		}

	}

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

		if unlikely(loop == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}

		if unlikely(!loop->running)
		{
			exception_set(err, EINVAL, "Loop not running!");
			return false;
		}
	}

	loop->running = false;
	return true;
}

bool mainloop_addwatcher(mainloop_t * loop, fdwatcher_t * watcher, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(loop == NULL || watcher == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}

		if unlikely(watcher->fd < 0)
		{
			exception_set(err, EINVAL, "Invalid file descriptor");
			return false;
		}

		if unlikely(watcher->loop != NULL)
		{
			exception_set(err, EINVAL, "Watcher already added to mainloop");
			return false;
		}
	}

	// Add watcher fd to epoll
	{
		struct epoll_event event;
		memset(&event, 0, sizeof(struct epoll_event));
		event.events = watcher->events;
		event.data.ptr = watcher;

		if (epoll_ctl(loop->epollfd, EPOLL_CTL_ADD, watcher->fd, &event) < 0)
		{
			exception_set(err, errno, "Could not configure epoll to watch fd %d: %s", watcher->fd, strerror(errno));
			return false;
		}
	}

	watcher->loop = loop;

	return watcher;
}

bool mainloop_removewatcher(fdwatcher_t * watcher, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(watcher == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}

		if unlikely(watcher->fd < 0)
		{
			exception_set(err, EINVAL, "Invalid file descriptor");
			return false;
		}

		if unlikely(watcher->loop == NULL)
		{
			exception_set(err, EINVAL, "Watcher not added to mainloop");
			return false;
		}
	}

	// Remove the watcher from epoll
	int success = 0;
	if ((success = epoll_ctl(watcher->loop->epollfd, EPOLL_CTL_DEL, watcher->fd, NULL)) < 0)
	{
		exception_set(err, errno, "Could not configure epoll to unwatch fd %d: %s", watcher->fd, strerror(errno));
	}

	close(watcher->fd);
	watcher->loop = NULL;
	watcher->fd = -1;

	return success == 0;
}


void watcher_newfd(fdwatcher_t * watcher, int fd, fdcond_t events, watch_f listener, void * userdata)
{
	watcher_init(watcher);
	watcher->fd = fd;
	watcher->events = events;
	watcher->listener = listener;
	watcher->userdata = userdata;
}

static bool mainloop_timerfddispatch(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{
	timerwatcher_t * timerwatcher = userdata;

	uint64_t timeouts = 0;
	ssize_t r = read(fd, &timeouts, sizeof(timeouts));
	if (r != sizeof(uint64_t))
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Could not read all data from timerfd %s. Read %zd/%zu bytes: %s", timerwatcher->name, r, sizeof(uint64_t), strerror(errno));
		}

		return true;
	}
	else if (timeouts > 1)
	{
		log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Timerfd %s has been unsynchronized. It has fired %" PRIu64 " times without being dispatched!", timerwatcher->name, timeouts);
	}

	return timerwatcher->listener(loop, timerwatcher->nanoseconds, timerwatcher->userdata);
}

static bool mainloop_eventfddispatch(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{
	eventwatcher_t * eventwatcher = userdata;

	eventfd_t counter = 0;
	if (eventfd_read(fd, &counter) < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Could not read all data from eventfd %s: %s", eventwatcher->name, strerror(errno));
		}

		return true;
	}

	return eventwatcher->listener(loop, counter, eventwatcher->userdata);
}

bool watcher_newtimer(timerwatcher_t * timerwatcher, const char * name, uint64_t nanoseconds, timerfd_f listener, void * userdata, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(timerwatcher == NULL || name == NULL || listener == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}
	}

	// Create the file descriptor
	int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if (fd == -1)
	{
		exception_set(err, errno, "Could not create timerfd %s: %s", name, strerror(errno));
		return false;
	}

	// Get the current time
	struct timespec now;
	if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
	{
		exception_set(err, errno, "Could not get system time to create timerfd %s: %s", name, strerror(errno));
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

	// Apply the timer to the file descriptor
	if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &timer, NULL) == -1)
	{
		exception_set(err, errno, "Could not set timerfd %s: %s", name, strerror(errno));
		return false;
	}

	memset(timerwatcher, 0, sizeof(timerwatcher_t));
	watcher_newfd(&timerwatcher->watcher, fd, FD_READ, mainloop_timerfddispatch, timerwatcher);
	timerwatcher->name = name;
	timerwatcher->nanoseconds = nanoseconds;
	timerwatcher->listener = listener;
	timerwatcher->userdata = userdata;

	return true;
}

bool watcher_newevent(eventwatcher_t * eventwatcher, const char * name, unsigned int initialvalue, eventfd_f listener, void * userdata, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if (eventwatcher == NULL || name == NULL || listener == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}
	}

	// Create the file descriptor
	int fd = eventfd(initialvalue, EFD_NONBLOCK);
	if (fd == -1)
	{
		exception_set(err, errno, "Could not create eventfd %s: %s", name, strerror(errno));
		return false;
	}

	memset(eventwatcher, 0, sizeof(eventwatcher_t));
	watcher_newfd(&eventwatcher->watcher, fd, FD_READ, mainloop_eventfddispatch, eventwatcher);
	eventwatcher->name = name;
	eventwatcher->listener = listener;
	eventwatcher->userdata = userdata;

	return true;
}
