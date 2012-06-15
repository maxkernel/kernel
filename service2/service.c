#include <errno.h>

#include <aul/list.h>
#include <aul/mutex.h>
#include <aul/mainloop.h>

#include <kernel.h>

#include <service.h>
#include "service-priv.h"



#define subsystem_foreach(subsystem, subsystems) \
	for (size_t __i = 0; ((subsystem) = &(subsystems)[__i])->name != NULL; __i++)

typedef struct
{
	const char * name;
	bool (*init)(exception_t ** err);
} subsystem_t;

static subsystem_t subsystems[] = {
	{"TCP", tcp_init},
//	{"UDP", udp_init},
	{NULL, NULL}		// Sentinel
};


static mainloop_t * serviceloop = NULL;
static list_t services;
static mutex_t services_lock;

static bool service_checktimeout(mainloop_t * loop, uint64_t nanoseconds, void * userdata)
{
	/*
	GHashTableIter itr;
	client_t * client = NULL;
	int64_t now = kernel_timestamp();

	mutex_lock(&service_lock);
	{
		g_hash_table_iter_init(&itr, client_table);
		while (g_hash_table_iter_next(&itr, NULL, (void **)&client))
		{
			if ((now - client->timeout_us) > SERVICE_TIMEOUT_US)
			{
				LOG(LOG_DEBUG, "Service client '%s' disconnected (timeout)", client->handle);

				g_hash_table_iter_remove(&itr);

				mutex_lock(&client->lock);
				{
					size_t i;
					for (i=0; i<PROTOCOL_NUM; i++)
					{
						service_freestream(client->streams[i]);
					}

					client->inuse = false;
				}
				mutex_unlock(&client->lock);
			}
		}
	}

	mutex_unlock(&service_lock);
*/
	return true;
}

static ssize_t service_descservice(const kobject_t * object, char * buffer, size_t length)
{
	// TODO - finish me
	return 0;
}

static void service_destroyservice(kobject_t * object)
{
	service_t * service = (service_t *)object;

	// TODO - free all registered clients!

	free(service->name);
	if (service->desc != NULL)
	{
		free(service->desc);
	}
}

service_t * service_newservice(const char * name, const char * desc, exception_t ** err)
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

	service_t * service = kobj_new("Service", name, service_descservice, service_destroyservice, sizeof(service_t));
	mutex_init(&service->service_lock, M_RECURSIVE);
	service->name = strdup(name);
	service->desc = (desc == NULL)? NULL : strdup(desc);
	list_init(&service->clients);

	list_add(&services, &service->service_list);

	return service;
}


static ssize_t service_descstream(const kobject_t * object, char * buffer, size_t length)
{
	// TODO - finish me
	return 0;
}

static void service_destroystream(kobject_t * object)
{
	stream_t * stream = (stream_t *)object;

}

stream_t * service_newstream(const char * name, size_t objectsize, streamsend_f sender, streamcheck_f checker, clientdestroy_f destroyer, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(name == NULL || sender == NULL || checker == NULL)
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

	size_t clientsize = sizeof(client_t) + objectsize;

	stream_t * stream = kobj_new("Service Stream", name, service_descstream, service_destroystream, sizeof(stream_t));
	stream->loop = serviceloop;
	mutex_init(&stream->stream_lock, M_RECURSIVE);
	stream->sender = sender;
	stream->checker = checker;
	list_init(&stream->clients);

	void * clients = malloc(clientsize * SERVICE_CLIENTS_PER_STREAM);
	memset(clients, 0, clientsize * SERVICE_CLIENTS_PER_STREAM);
	for (size_t i = 0; i < SERVICE_CLIENTS_PER_STREAM; i++)
	{
		client_t * client = clients + (i * clientsize);
		client->destroyer = destroyer;
		list_add(&stream->clients, &client->stream_list);
	}

	return stream;
}

static bool service_runloop(void * userdata)
{
	mainloop_t * serviceloop = userdata;

	exception_t * e = NULL;
	if (!mainloop_run(serviceloop, &e))
	{
		LOG(LOG_ERR, "Could not run service mainloop: %s", exception_message(e));
		exception_free(e);
	}

	return false;
}

static bool service_stoploop(void * userdata)
{
	mainloop_t * serviceloop = userdata;

	exception_t * e = NULL;
	if (!mainloop_stop(serviceloop, &e))
	{
		LOG(LOG_ERR, "Could not stop service mainloop: %s", exception_message(e));
		exception_free(e);
		return false;
	}

	return true;
}

static void service_preactivate()
{
	list_init(&services);
	mutex_init(&services_lock, M_RECURSIVE);

	/*
	memset(clients, 0, sizeof(client_t) * SERVICE_CLIENTS_MAX);

	mutex_init(&service_lock, M_RECURSIVE);
	service_table = g_hash_table_new(g_str_hash, g_str_equal);
	client_table = g_hash_table_new(g_str_hash, g_str_equal);
	stream_table = g_hash_table_new(g_str_hash, g_str_equal);
	*/
}

static bool service_init()
{
	LOG(LOG_DEBUG, "Initializing service subsystem");

	// Initialize the mainloop
	{
		exception_t * e = NULL;
		serviceloop = mainloop_new("Service network loop", &e);
		if (serviceloop == NULL || exception_check(&e))
		{
			LOG(LOG_ERR, "Could not create service mainloop: %s", exception_message(e));
			exception_free(e);
			serviceloop = NULL;
			return false;
		}

		if (mainloop_newfdtimer(serviceloop, "Service monitor", SERVICE_TIMEOUT_NS, service_checktimeout, serviceloop, &e) < 0)
		{
			LOG(LOG_ERR, "Could not create service monitor timer: %s", exception_message(e));
			exception_free(e);
			return false;
		}

		if (!kthread_newthread("Service handler", KTH_PRIO_LOW, service_runloop, service_stoploop, NULL, &e))
		{
			LOG(LOG_ERR, "Could not start service server thread!");
			exception_free(e);
			return false;
		}
	}

	// Initialize the subsystems
	{
		const subsystem_t * subsystem = NULL;
		subsystem_foreach(subsystem, subsystems)
		{
			exception_t * e = NULL;
			if (!subsystem->init(&e))
			{
				LOG(LOG_ERR, "Could not initialize service %s subsystem: %s", subsystem->name, exception_message(e));
				exception_free(e);
			}
		}
	}

	return true;
}


module_name("Service");
module_version(0,8,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Provides a publish/subscribe read-only API to help modules stream sensors/status/other stuff to external clients");
module_onpreactivate(service_preactivate);
module_oninitialize(service_init);
