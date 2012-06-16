#include <errno.h>

#include <aul/common.h>
#include <aul/exception.h>
#include <aul/mutex.h>

#include <kernel.h>

#include <service.h>
#include "service-priv.h"


client_t * stream_newclient(stream_t * stream, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(stream == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}

	client_t * client = NULL;

	mutex_lock(&stream->stream_lock);
	{
		list_t * pos = NULL;
		list_foreach(pos, &stream->clients)
		{
			client_t * testclient = list_entry(pos, client_t, stream_list);
			if (testclient->service == NULL)
			{
				client = testclient;
				client->inuse = true;
				break;
			}
		}
	}
	mutex_unlock(&stream->stream_lock);

	if (client == NULL)
	{
		exception_set(err, ENOMEM, "No free clients!");
		return NULL;
	}

	client->lastheartbeat = kernel_elapsed();
	return client;
}

void stream_freeclient(client_t * client)
{
	// Sanity check
	{
		if unlikely(client == NULL)
		{
			return;
		}
	}

	// Unsubscribe for service (if registered)
	{
		service_t * service = client_service(client);
		if (service != NULL)
		{
			service_unsubscribe(client);
		}
	}

	// Call the destroyer
	{
		clientdestroy_f destroyer = client_destroyer(client);
		if (destroyer != NULL)
		{
			destroyer(client);
		}
	}

	// Set the inuse flag
	{
		stream_t * stream = client_stream(client);
		mutex_lock(&stream->stream_lock);
		{
			client_inuse(client) = false;
		}
		mutex_unlock(&stream->stream_lock);
	}
}
