
#include <aul/string.h>
#include <aul/mainloop.h>
#include <aul/mutex.h>

#include <kernel.h>
#include <service.h>
#include "internal.h"

#define SERVICE_CLASS		"Service"
#define SERVICE_NAME		"service"

extern GHashTable * service_table;
extern GHashTable * stream_table;


static ssize_t service_info(kobject_t * object, char * buffer, size_t length)
{
	return 0;
}

static void service_free(kobject_t * object)
{
	service_t * s = (service_t *)object;

	mutex_lock(&service_lock);
	{
		g_hash_table_remove(service_table, s->handle);
	}
	mutex_unlock(&service_lock);

	if (s->name != NULL)	free(s->name);
	if (s->format != NULL)	free(s->format);
	if (s->desc != NULL)	free(s->desc);
}


void service_dispatchdata(service_h service_handle, client_h client_handle, stream_h stream_handle, uint64_t timestamp_us, void * data, size_t len)
{
	stream_t * stream = NULL;
	service_t * service = NULL;

	mutex_lock(&service_lock);
	{
		service = g_hash_table_lookup(service_table, service_handle);
		stream = g_hash_table_lookup(stream_table, stream_handle);
	}
	mutex_unlock(&service_lock);

	if (service == NULL)
	{
		LOG(LOG_WARN, "Service client (%s) attempted to write to invalid service (%s)", client_handle, service_handle);
		return;
	}

	if (stream == NULL)
	{
		LOG(LOG_WARN, "Stream client (%s) attempted to write from invalid stream (%s)", client_handle, stream_handle);
		return;
	}

	mutex_lock(&service->lock);
	{
		size_t i;
		bool found = false;

		for (i=0; i<STREAMS_PER_SERVICE; i++)
		{
			if (service->streams[i] == stream)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			LOG(LOG_WARN, "Service client (%s) attempted to illegally write to service (%s)", client_handle, service_handle);
		}
		else if (service->clientdata == NULL)
		{
			LOG(LOG_WARN, "Service client (%s) attempted to write to read-only service (%s)", client_handle, service_handle);
		}
		else
		{
			service->clientdata(service_handle, stream_handle, timestamp_us, data, len);
		}
	}
	mutex_unlock(&service->lock);
}

void service_startstream(service_h service_handle, stream_h stream_handle)
{
	stream_t * stream = NULL;
	service_t * service = NULL;

	mutex_lock(&service_lock);
	{
		service = g_hash_table_lookup(service_table, service_handle);
		stream = g_hash_table_lookup(stream_table, stream_handle);
	}
	mutex_unlock(&service_lock);

	if (stream == NULL)
	{
		LOG(LOG_WARN, "Unknown stream client attempted to start streaming service (%s) from invalid stream (%s)", service_handle, stream_handle);
		return;
	}

	if (stream->client == NULL)
	{
		LOG(LOG_WARN, "Unknown stream client attempted to start streaming service (%s) from stream (%s)", service_handle, stream_handle);
		return;
	}

	if (service == NULL)
	{
		LOG(LOG_WARN, "Service client (%s) attempted to start streaming invalid service (%s)", stream->client->handle, service_handle);
		return;
	}

	mutex_lock(&service->lock);
	{
		size_t i;
		bool found = false;

		for (i=0; i<STREAMS_PER_SERVICE; i++)
		{
			if (service->streams[i] == NULL)
			{
				service->streams[i] = stream;
				found = true;
				break;
			}
		}

		if (!found)
		{
			LOG(LOG_WARN, "Could not start streaming service (%s) to client (%s), SERVICE_CLIENTS_MAX (%d) exceeded", service_handle, stream->client->handle, SERVICE_CLIENTS_MAX);
		}
		else
		{
			if (service->connect != NULL)
			{
				service->connect(service_handle, stream_handle);
			}

			LOG(LOG_DEBUG, "Started streaming service (%s) to client (%s)", service_handle, stream->client->handle);
		}
	}
	mutex_unlock(&service->lock);
}

void service_stopstream(service_h service_handle, stream_h stream_handle)
{
	stream_t * stream = NULL;
	service_t * service = NULL;

	mutex_lock(&service_lock);
	{
		service = g_hash_table_lookup(service_table, service_handle);
		stream = g_hash_table_lookup(stream_table, stream_handle);
	}
	mutex_unlock(&service_lock);

	if (stream == NULL)
	{
		LOG(LOG_WARN, "Unknown stream client attempted to stop streaming service (%s) from invalid stream (%s)", service_handle, stream_handle);
		return;
	}

	if (stream->client == NULL)
	{
		LOG(LOG_WARN, "Unknown stream client attempted to stop streaming service (%s) from stream (%s)", service_handle, stream_handle);
		return;
	}

	if (service == NULL)
	{
		LOG(LOG_WARN, "Service client (%s) attempted to stop streaming invalid service (%s)", stream->client->handle, service_handle);
		return;
	}

	mutex_lock(&service->lock);
	{
		size_t i;
		bool found = false;

		for (i=0; i<STREAMS_PER_SERVICE; i++)
		{
			if (service->streams[i] == stream)
			{
				service->streams[i] = NULL;
				found = true;
				break;
			}
		}

		if (!found)
		{
			LOG(LOG_WARN, "Could not stop streaming service (%s) to client (%s), stream never existed.", service_handle, stream->client->handle);
		}
		else
		{
			if (service->disconnect != NULL)
			{
				service->disconnect(service_handle, stream_handle);
			}

			LOG(LOG_DEBUG, "Stopped streaming service (%s) to client (%s)", service_handle, stream->client->handle);
		}
	}
	mutex_unlock(&service->lock);
}

service_h service_register(const char * id, const char * name, const char * format, const char * params, const char * desc, connect_f newconnect, disconnect_f disconnected, clientdata_f clientdata)
{
	string_t kobj_name = string_new("%s " SERVICE_NAME, name);
	service_t * s = kobj_new(SERVICE_CLASS, kobj_name.string, service_info, service_free, sizeof(service_t));
	s->name = (name == NULL)? NULL : strdup(name);
	s->format = (format == NULL)? NULL : strdup(format);
	s->desc = (desc == NULL)? NULL : strdup(desc);
	s->connect = newconnect;
	s->disconnect = disconnected;
	s->clientdata = clientdata;

	mutex_init(&s->lock, M_RECURSIVE);

	//prepare id
	if (id != NULL)
	{
		if (strlen(id) >= HANDLE_MAXLEN)
		{
			LOG(LOG_WARN, "Service ID %s for service %s is too long, truncating (max length = %d)", id, name, HANDLE_MAXLEN-1);
		}

		strncpy(s->handle, id, HANDLE_MAXLEN);
		s->handle[HANDLE_MAXLEN-1] = 0;

		if (!isunique(s->handle, service_table))
		{
			LOG(LOG_WARN, "Service ID %s for service %s is not unique. Generating a new one", id, name);
			generateid(s->handle, service_table);
		}
	}
	else
	{
		generateid(s->handle, service_table);
	}

	//parse params
	{
		//TODO - do the parsing
	}

	LOG(LOG_DEBUG, "Registered service '%s' with id '%s' using format %s", s->name, s->handle, s->format);

	mutex_lock(&service_lock);
	{
		g_hash_table_insert(service_table, s->handle, s);
	}
	mutex_unlock(&service_lock);

	return s->handle;
}

void service_writedata(service_h service_handle, uint64_t timestamp_us, const buffer_t buffer)
{
	service_t * service = NULL;

	mutex_lock(&service_lock);
	{
		service = g_hash_table_lookup(service_table, service_handle);
	}
	mutex_unlock(&service_lock);

	if (service == NULL)
	{
		LOG(LOG_WARN, "Trying to write to service (%s) that doesn't exist", service_handle);
		return;
	}

	mutex_lock(&service->lock);
	{
		size_t i=0;
		for (; i<STREAMS_PER_SERVICE; i++)
		{
			if (service->streams[i] != NULL)
			{
				send_data(service_handle, service->streams[i]->client->handle, service->streams[i], timestamp_us, buffer);
			}
		}
	}
	mutex_unlock(&service->lock);
}

void service_writeclientdata(service_h service_handle, stream_h stream_handle, uint64_t timestamp_us, const buffer_t buffer)
{
	service_t * service = NULL;
	stream_t * stream = NULL;

	mutex_lock(&service_lock);
	{
		service = g_hash_table_lookup(service_table, service_handle);
		stream = g_hash_table_lookup(stream_table, stream_handle);
	}
	mutex_unlock(&service_lock);

	if (service == NULL)
	{
		LOG(LOG_WARN, "Trying to write to data of service (%s) that doesn't exist", service_handle);
		return;
	}

	if (stream == NULL)
	{
		LOG(LOG_WARN, "Trying to write to data of service (%s) to stream (%s) that doesn't exist", service_handle, stream_handle);
		return;
	}

	mutex_lock(&service->lock);
	{
		bool found = false;
		size_t i=0;
		for (; i<STREAMS_PER_SERVICE; i++)
		{
			if (service->streams[i] != stream)
			{
				found = true;
				break;
			}
		}

		if (found)
		{
			send_data(service_handle, stream->client->handle, stream, timestamp_us, buffer);
		}
		else
		{
			LOG(LOG_WARN, "Trying to write data of service (%s) to stream (%s) that isn't registered with that service", service_handle, stream_handle);
		}
	}
	mutex_unlock(&service->lock);
}
