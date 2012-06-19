#include <errno.h>

#include <kernel.h>

#include <service.h>
#include "service-priv.h"


client_t * client_new(stream_t * stream, exception_t ** err)
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

	mutex_lock(&stream->lock);
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
	mutex_unlock(&stream->lock);

	if (client == NULL)
	{
		exception_set(err, ENOMEM, "No free clients!");
		return NULL;
	}

	client->lastheartbeat = kernel_elapsed();
	return client;
}

void client_destroy(client_t * client)
{
	// Sanity check
	{
		if unlikely(client == NULL)
		{
			return;
		}
	}

	mutex_lock(client_lock(client));
	{
		if (client_inuse(client))
		{
			// Unsubscribe for service (if registered)
			service_t * service = client_service(client);
			if (service != NULL)
			{
				service_unsubscribe(client);
			}

			// Call the destroyer
			clientdestroy_f destroyer = client->destroyer;
			if (destroyer != NULL)
			{
				destroyer(client);
			}

			// Set the inuse flag
			client_inuse(client) = false;
		}
	}
	mutex_unlock(client_lock(client));
}

bool client_check(client_t * client)
{
	bool check = true;
	mutex_lock(client_lock(client));
	{
		check = client->checker(client);
	}
	mutex_unlock(client_lock(client));

	return check;
}

ssize_t clienthelper_control(client_t * client, void * buffer, size_t length)
{
	// Sanity check
	{
		if unlikely(client == NULL)
		{
			return -1;
		}
	}

	if (length < sizeof(uint8_t))
	{
		// Nothing to read
		return 0;
	}

	uint8_t code = *(uint8_t *)buffer;
	switch (code)
	{
		case SC_GOODBYE:
		{
			return -1;
		}

		case SC_AUTH:
		{
			LOG(LOG_INFO, "Service client AUTH not supported yet.");
			return -1;
		}

		case SC_HEARTBEAT:
		{
			client_lastheartbeat(client) = kernel_elapsed();
			return sizeof(uint8_t);
		}

		case SC_DATA:
		{
			LOG(LOG_WARN, "Invalid control code received from stream client (SC_DATA)");
			return -1;
		}

		case SC_SUBSCRIBE:
		{
			buffer += sizeof(uint8_t);
			length -= sizeof(uint8_t);

			char * name = (char *)buffer;
			size_t strlen = strnlen(name, length);
			if (strlen == length)
			{
				// Not full command in buffer, return 0 to indicate nothing to do
				return 0;
			}

			service_t * service = service_lookup(name);
			if (service == NULL)
			{
				LOG(LOG_WARN, "Attempting to subscribe client to invalid service %s", name);
				return -1;
			}

			exception_t * e = NULL;
			if (!service_subscribe(service, client, &e))
			{
				LOG(LOG_WARN, "Could not subscribe client to service %s: %s", name, exception_message(e));
				exception_free(e);

				return -1;
			}

			return sizeof(uint8_t) + (strlen + 1);
		}

		case SC_UNSUBSCRIBE:
		{
			service_unsubscribe(client);
			return sizeof(uint8_t);
		}

		case SC_LISTXML:
		{
			bool success = false;
			buffer_t buffer = buffer_new();
			{
				service_listxml(&buffer);
				success = client_send(client, kernel_timestamp(), &buffer);
			}
			buffer_free(buffer);
			return (success)? sizeof(uint8_t) : -1;
		}

		default:
		{
			LOG(LOG_WARN, "Unknown control code received from stream client: %#x", code);
			return -1;
		}
	}
}
