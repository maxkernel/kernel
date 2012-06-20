#include <errno.h>

#include <aul/common.h>
#include <aul/exception.h>
#include <aul/mutex.h>
#include <aul/stack.h>

#include <kernel.h>

#include <service.h>
#include "service-priv.h"

extern mainloop_t * serviceloop;

static ssize_t stream_kobjdesc(const kobject_t * object, char * buffer, size_t length)
{
	// TODO - add some description, maybe.
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

stream_t * stream_new(const char * name, size_t streamsize, streamdestroy_f sdestroyer, size_t clientsize, clientsend_f csender, clientcheck_f cchecker, clientdestroy_f cdestroyer, exception_t ** err)
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

		if unlikely(serviceloop == NULL)
		{
			exception_set(err, EFAULT, "Service mainloop not initialized");
			return NULL;
		}
	}

	stream_t * stream = kobj_new("Service Stream", name, stream_kobjdesc, stream_kobjdestroy, sizeof(stream_t) + streamsize);
	stream->loop = serviceloop;
	stream->destroyer = sdestroyer;
	mutex_init(&stream->lock, M_RECURSIVE);
	list_init(&stream->clients);

	for (size_t i = 0; i < SERVICE_CLIENTS_PER_STREAM; i++)
	{
		client_t * client = malloc(sizeof(client_t) + clientsize);
		memset(client, 0, sizeof(client_t) + clientsize);
		client->lock = &stream->lock;
		client->sender = csender;
		client->checker = cchecker;
		client->destroyer = cdestroyer;
		list_add(&stream->clients, &client->stream_list);
	}

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
