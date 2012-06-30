#include <stdio.h>
#include <errno.h>

#include <aul/list.h>
#include <aul/stack.h>
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
	void (*preact)();
	bool (*init)(exception_t ** err);
} subsystem_t;

static subsystem_t subsystems[] = {
	{"Stream", stream_preact, stream_init},
	{"TCP", NULL, tcp_init},
	{"UDP", NULL, udp_init},
	{NULL, NULL, NULL}		// Sentinel
};

static list_t services;
static mutex_t services_lock;

static stack_t packets;
static stack_t packets_free;
static mutex_t packets_lock;
static cond_t packets_barrier;

static volatile bool stop = false;
static int dispatch_threads = 1;
static timerwatcher_t monitor_watcher;

static bool service_monitor(mainloop_t * loop, uint64_t nanoseconds, void * userdata)
{
	mutex_lock(&services_lock);
	{
		list_t * pos = NULL;
		list_foreach(pos, &services)
		{
			service_t * service = list_entry(pos, service_t, service_list);

			mutex_lock(&service->lock);
			{
				list_t * pos = NULL, * n = NULL;
				list_foreach_safe(pos, n, &service->clients)
				{
					client_t * client = list_entry(pos, client_t, service_list);

					// Run the heartbeat function
					{
						//LOG(LOG_INFO, "SEND HEARTBEAT");

						clientheartbeat_f heartbeater = client->heartbeater;
						if (heartbeater != NULL)
						{
							heartbeater(client);
						}
					}
				}
			}
			mutex_unlock(&service->lock);
		}
	}
	mutex_unlock(&services_lock);

	return true;
}

static bool service_rundispatch(void * userdata)
{
	while (!stop)
	{
		list_t * packet_entry = NULL;

		mutex_lock(&packets_lock);
		{
			cond_wait(&packets_barrier, &packets_lock, 0);
			packet_entry = stack_pop(&packets);
		}
		mutex_unlock(&packets_lock);

		while (!stop && packet_entry != NULL)
		{
			packet_t * packet = list_entry(packet_entry, packet_t, packet_list);
			service_t * service = packet->service;

			mutex_lock(&service->lock);
			{
				list_t * pos = NULL, * n = NULL;
				list_foreach_safe(pos, n, &service->clients)
				{
					client_t * client = list_entry(pos, client_t, service_list);
					if (!client_sender(client)(client, packet->timestamp, packet->buffer))
					{
						// Failure to send data to client, drop client
						LOG(LOG_DEBUG, "Failure to send data to client");
						client_destroy(client);
					}
				}
			}
			mutex_unlock(&service->lock);

			// Free the buffer in the packet
			buffer_free(packet->buffer);

			// Add the packet back to the free list, attempt to grab another packet
			mutex_lock(&packets_lock);
			{
				stack_push(&packets_free, &packet->packet_list);
				packet_entry = stack_pop(&packets);
			}
			mutex_unlock(&packets_lock);
		}
	}

	return false;
}

static bool service_stopdispatch(void * userdata)
{
	stop = true;

	mutex_lock(&packets_lock);
	{
		cond_broadcast(&packets_barrier);
	}
	mutex_unlock(&packets_lock);

	return true;
}

service_t * service_lookup(const char * name)
{
	// Sanity check
	{
		if unlikely(name == NULL)
		{
			return NULL;
		}
	}

	service_t * service = NULL;

	mutex_lock(&services_lock);
	{
		list_t * pos = NULL;
		list_foreach(pos, &services)
		{
			service_t * testservice = list_entry(pos, service_t, service_list);
			if (strcmp(service_name(testservice), name) == 0)
			{
				service = testservice;
				break;
			}
		}
	}
	mutex_unlock(&services_lock);

	return service;
}

void service_send(service_t * service, int64_t microtimestamp, const buffer_t * buffer)
{
	// Sanity check
	{
		if unlikely(service == NULL || buffer == NULL)
		{
			return;
		}
	}

	// Get a free packet
	list_t * entry = NULL;
	mutex_lock(&packets_lock);
	{
		entry = stack_pop(&packets_free);
	}
	mutex_unlock(&packets_lock);

	if (entry == NULL)
	{
		LOG(LOG_WARN, "Out of free service packets!");
		return;
	}

	// Populate the packet
	packet_t * packet = list_entry(entry, packet_t, packet_list);
	memset(packet, 0, sizeof(packet_t));
	packet->service = service;
	packet->timestamp = microtimestamp;
	packet->buffer = buffer_dup(buffer);

	// Add the packet to the consumer-list and notify the dispatch threads
	mutex_lock(&packets_lock);
	{
		stack_enqueue(&packets, &packet->packet_list);
		cond_signal(&packets_barrier);
	}
	mutex_unlock(&packets_lock);
}

bool service_subscribe(service_t * service, client_t * client, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(service == NULL || client == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}

		if (!client_inuse(client) || client_locked(client))
		{
			exception_set(err, EINVAL, "Attempting to subscribe to service with invalid client");
			return false;
		}
	}

	mutex_lock(&service->lock);
	{
		list_add(&service->clients, &client->service_list);
	}
	mutex_unlock(&service->lock);

	return true;
}

void service_unsubscribe(client_t * client)
{
	// Sanity check
	{
		if unlikely(client == NULL)
		{
			return;
		}
	}

	service_t * service = client_service(client);
	if (service != NULL)
	{
		mutex_lock(&service->lock);
		{
			list_remove(&client->service_list);
		}
		mutex_unlock(&service->lock);
	}
}

static ssize_t service_kobjdesc(const kobject_t * object, char * buffer, size_t length)
{
	service_t * service = (service_t *)object;
	return snprintf(buffer, length, "{ 'name': '%s', 'format': '%s', 'description': '%s' }", service->name, service->format, service->desc);
}

static void service_kobjdestroy(kobject_t * object)
{
	service_t * service = (service_t *)object;

	// Destroy all registered clients (prevent them from entering unknown state)
	mutex_lock(&service->lock);
	{
		list_t * pos = NULL, * n = NULL;
		list_foreach_safe(pos, n, &service->clients)
		{
			client_t * client = list_entry(pos, client_t, service_list);
			client_destroy(client);
		}
	}
	mutex_unlock(&service->lock);

	free(service->name);
	free(service->format);
	if (service->desc != NULL)
	{
		free(service->desc);
	}

	mutex_lock(&services_lock);
	{
		list_remove(&service->service_list);
	}
	mutex_unlock(&services_lock);
}

service_t * service_new(const char * name, const char * format, const char * desc, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(name == NULL || format == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}

	// Test for existing service
	{
		service_t * existing = service_lookup(name);
		if (existing != NULL)
		{
			exception_set(err, EEXIST, "Service by the name of '%s' already exists", name);
			return NULL;
		}
	}

	service_t * service = kobj_new("Service", name, service_kobjdesc, service_kobjdestroy, sizeof(service_t));
	mutex_init(&service->lock, M_RECURSIVE);
	service->name = strdup(name);
	service->format = strdup(format);
	service->desc = (desc == NULL)? NULL : strdup(desc);
	list_init(&service->clients);

	list_add(&services, &service->service_list);

	return service;
}

void service_destroy(service_t * service)
{
	// Sanity check
	{
		if unlikely(service == NULL)
		{
			return;
		}
	}

	kobj_destroy(kobj_cast(service));
}

void service_listxml(buffer_t * buffer)
{
	// Sanity check
	{
		if unlikely(buffer == NULL)
		{
			return;
		}
	}

	size_t bufferlen = 0;

	void append(const char * fmt, ...)
	{
		string_t str = string_blank();

		va_list args;
		va_start(args, fmt);
		string_vset(&str, fmt, args);
		va_end(args);

		buffer_write(buffer, str.string, bufferlen, str.length);
		bufferlen += str.length;
	}

	append("<servicelist>");
	mutex_lock(&services_lock);
	{
		list_t * pos = NULL;
		list_foreach(pos, &services)
		{
			service_t * service = list_entry(pos, service_t, service_list);
			append("<service name=\"%s\" format=\"%s\"", service_name(service), service_format(service));
			if (service_desc(service) != NULL)
			{
				append(" description=\"%s\"", service_desc(service));
			}
			append(" />");
		}
	}
	mutex_unlock(&services_lock);
	append("</servicelist>");
}

static void service_preactivate()
{
	list_init(&services);
	mutex_init(&services_lock, M_RECURSIVE);

	stack_init(&packets);
	stack_init(&packets_free);
	mutex_init(&packets_lock, M_RECURSIVE);
	cond_init(&packets_barrier);

	for (size_t i = 0; i < SERVICE_NUM_PACKETS; i++)
	{
		packet_t * packet = malloc(sizeof(packet_t));
		memset(packet, 0, sizeof(packet_t));
		stack_push(&packets_free, &packet->packet_list);
	}

	// Preact the subsystems
	{
		const subsystem_t * subsystem = NULL;
		subsystem_foreach(subsystem, subsystems)
		{
			if (subsystem->preact != NULL)
			{
				subsystem->preact();
			}
		}
	}
}

static bool service_init()
{
	LOG(LOG_DEBUG, "Initializing service subsystem");

	// Initialize the service monitor timer
	{
		exception_t * e = NULL;
		if (!watcher_newtimer(&monitor_watcher, "Service monitor", SERVICE_MONITOR_TIMEOUT, service_monitor, NULL, &e) || exception_check(&e))
		{
			LOG(LOG_ERR, "Could not create service monitor timer: %s", exception_message(e));
			exception_free(e);
			return false;
		}

		if (!mainloop_addwatcher(kernel_mainloop(), watcher_cast(&monitor_watcher), &e) || exception_check(&e))
		{
			LOG(LOG_ERR, "Could not add service monitor timer to mainloop: %s", exception_message(e));
			exception_free(e);
			return false;
		}
	}

	// Initialize the dispatch threads
	{
		if (dispatch_threads <= 0)
		{
			LOG(LOG_ERR, "Invalid number of service dispatch threads: %d!", dispatch_threads);
			return false;
		}

		for (size_t i = 0; i < dispatch_threads; i++)
		{
			exception_t * e = NULL;
			string_t name = string_new("Service stream dispatch %zu", i+1);
			if (!kthread_newthread(name.string, KTH_PRIO_LOW, service_rundispatch, service_stopdispatch, NULL, &e))
			{
				LOG(LOG_ERR, "Could not start service dispatch server thread %zu!", (i+1));
				exception_free(e);
				return false;
			}
		}
	}

	// Initialize the subsystems
	{
		const subsystem_t * subsystem = NULL;
		subsystem_foreach(subsystem, subsystems)
		{
			exception_t * e = NULL;
			if (subsystem->init != NULL && !subsystem->init(&e))
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

module_config(dispatch_threads, T_INTEGER, "Number of service packet dispatch threads. Usually just one is needed, but on a system with many services, increase for better throughput.");
