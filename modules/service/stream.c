#include <errno.h>

#include <aul/common.h>
#include <aul/exception.h>
#include <aul/mutex.h>
#include <aul/stack.h>

#include <kernel.h>

#include <service.h>
#include "service-priv.h"


static mainloop_t * streamloop;

static list_t streams;
static mutex_t streams_lock;
static timerwatcher_t monitor_watcher;


static bool stream_monitor(mainloop_t * loop, uint64_t nanoseconds, void * userdata)
{
	mutex_lock(&streams_lock);
	{
		list_t * pos = NULL, * n = NULL;
		list_foreach_safe(pos, n, &streams)
		{
			stream_t * stream = list_entry(pos, stream_t, stream_list);

			mutex_lock(&stream->lock);
			{
				list_t * pos = NULL, * n = NULL;
				list_foreach_safe(pos, n, &stream->clients)
				{
					client_t * client = list_entry(pos, client_t, stream_list);
					if (client_inuse(client) && !client->checker(client))
					{
						LOG(LOG_DEBUG, "Service client destroyed because of failed check");
						client_destroy(client);
					}
				}
			}
			mutex_unlock(&stream->lock);
		}
	}
	mutex_unlock(&streams_lock);

	return true;
}

static ssize_t stream_kobjdesc(const kobject_t * object, char * buffer, size_t length)
{
	const stream_t * stream = (const stream_t *)object;
	if (stream->info != NULL)
	{
		return stream->info(stream, buffer, length);
	}

	return 0;
}

static void stream_kobjdestroy(kobject_t * object)
{
	stream_t * stream = (stream_t *)object;

	mutex_lock(&stream->lock);
	{
		list_t * pos = NULL, * n = NULL;
		list_foreach_safe(pos, n, &stream->clients)
		{
			client_t * client = list_entry(pos, client_t, stream_list);
			if (client_inuse(client))
			{
				client_destroy(client);
			}
		}
	}
	mutex_unlock(&stream->lock);

	// Call destroy function
	if (stream->destroyer != NULL)
	{
		stream->destroyer(stream);
	}

	// Now free the memory in the clients list
	list_t * entry = NULL;
	while ((entry = stack_pop(&stream->clients)) != NULL)
	{
		client_t * client = list_entry(entry, client_t, stream_list);
		free(client);
	}
}

stream_t * stream_new(const char * name, size_t streamsize, streamdesc_f sdesc,streamdestroy_f sdestroyer, size_t clientsize, clientsend_f csender, clientheartbeat_f cheartbeater, clientcheck_f cchecker, clientdestroy_f cdestroyer, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(name == NULL || csender == NULL || cchecker == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}

		if unlikely(streamloop == NULL)
		{
			exception_set(err, EFAULT, "Service mainloop not initialized");
			return NULL;
		}
	}

	stream_t * stream = kobj_new("Service Stream", name, stream_kobjdesc, stream_kobjdestroy, sizeof(stream_t) + streamsize);
	stream->loop = streamloop;
	stream->info = sdesc;
	stream->destroyer = sdestroyer;
	mutex_init(&stream->lock, M_RECURSIVE);
	list_init(&stream->clients);

	for (size_t i = 0; i < SERVICE_CLIENTS_PER_STREAM; i++)
	{
		client_t * client = malloc(sizeof(client_t) + clientsize);
		memset(client, 0, sizeof(client_t) + clientsize);
		client->stream = stream;
		client->lock = &stream->lock;
		client->sender = csender;
		client->heartbeater = cheartbeater;
		client->checker = cchecker;
		client->destroyer = cdestroyer;
		list_add(&stream->clients, &client->stream_list);
	}

	// Add stream to list
	mutex_lock(&streams_lock);
	{
		list_add(&streams, &stream->stream_list);
	}
	mutex_unlock(&streams_lock);

	return stream;
}

void stream_destroy(stream_t * stream)
{
	// Free all the clients registered with services
	mutex_lock(&stream->lock);
	{
		list_t * pos = NULL, * n = NULL;
		list_foreach_safe(pos, n, &stream->clients)
		{
			client_t * client = list_entry(pos, client_t, stream_list);
			if (client_inuse(client))
			{
				client_destroy(client);
			}
		}
	}
	mutex_unlock(&stream->lock);

	// Destroy the kobject
	kobj_destroy(kobj_cast(stream));
}

static bool stream_runloop(void * userdata)
{
	mainloop_t * streamloop = userdata;

	exception_t * e = NULL;
	if (!mainloop_run(streamloop, &e))
	{
		LOG(LOG_ERR, "Could not run service stream mainloop: %s", exception_message(e));
		exception_free(e);
	}

	return false;
}

static bool stream_stoploop(void * userdata)
{
	mainloop_t * streamloop = userdata;

	exception_t * e = NULL;
	if (!mainloop_stop(streamloop, &e))
	{
		LOG(LOG_ERR, "Could not stop service stream mainloop: %s", exception_message(e));
		exception_free(e);
		return false;
	}

	return true;
}

void stream_preact()
{
	list_init(&streams);
	mutex_init(&streams_lock, M_RECURSIVE);
}

bool stream_init(exception_t ** err)
{
	// Create the mainloop
	{
		streamloop = mainloop_new("Service stream network loop", err);
		if (streamloop == NULL || exception_check(err))
		{
			streamloop = NULL;
			return false;
		}
	}

	// Create the stream monitor timer
	{
		if (!watcher_newtimer(&monitor_watcher, "Service stream monitor", STREAM_MONITOR_TIMEOUT, stream_monitor, NULL, err) || exception_check(err))
		{
			return false;
		}

		if (!mainloop_addwatcher(streamloop, watcher_cast(&monitor_watcher), err) || exception_check(err))
		{
			return false;
		}
	}

	// Create the mainloop thread
	{
		if (!kthread_newthread("Service stream handler", KTH_PRIO_LOW, stream_runloop, stream_stoploop, streamloop, err))
		{
			return false;
		}
	}

	return true;
}
