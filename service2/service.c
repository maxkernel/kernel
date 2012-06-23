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
	bool (*init)(exception_t ** err);
} subsystem_t;

static subsystem_t subsystems[] = {
	{"TCP", tcp_init},
//	{"UDP", udp_init},
	{NULL, NULL}		// Sentinel
};


mainloop_t * serviceloop = NULL;
int serviceeventfd = -1;

static list_t services;
static mutex_t services_lock;

static stack_t packets;
static stack_t packets_free;
static mutex_t packets_lock;

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
						LOG(LOG_INFO, "SEND CHECK");

						clientheartbeat_f heartbeater = client->heartbeater;
						if (heartbeater != NULL)
						{
							heartbeater(client);
						}
					}

					// Run the check function
					{
						if (!client->checker(client))
						{
							LOG(LOG_INFO, "Destroy because of checker");
							client_destroy(client);
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

static bool service_dispatch(mainloop_t * loop, eventfd_t counter, void * userdata)
{
	list_t * entry = NULL;

	do
	{
		// Pop a packet from the list
		mutex_lock(&packets_lock);
		{
			entry = stack_pop(&packets);
		}
		mutex_unlock(&packets_lock);

		if (entry != NULL)
		{
			packet_t * packet = list_entry(entry, packet_t, packet_list);
			service_t * service = packet->service;

			// Send the packet to all client handlers for transmission
			mutex_lock(&service->lock);
			{
				list_t * pos = NULL, * n = NULL;
				list_foreach_safe(pos, n, &service->clients)
				{
					client_t * client = list_entry(pos, client_t, service_list);
					client_send(client, packet->timestamp, packet->buffer);
				}
			}
			mutex_unlock(&service->lock);

			// Free the buffer
			buffer_free(packet->buffer);

			// Add the packet back to the free list
			mutex_lock(&packets_lock);
			{
				stack_push(&packets_free, &packet->packet_list);
			}
			mutex_unlock(&packets_lock);
		}

	} while (entry != NULL);

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

	// Add the packet to the consumer-list
	mutex_lock(&packets_lock);
	{
		stack_enqueue(&packets, &packet->packet_list);
	}
	mutex_unlock(&packets_lock);

	// Notify the consumer eventfd
	if (eventfd_write(serviceeventfd, 1) < 0)
	{
		LOG(LOG_WARN, "Could not notify service sender eventfd of new data");
	}
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

	stack_init(&packets);
	stack_init(&packets_free);
	mutex_init(&packets_lock, M_RECURSIVE);

	for (size_t i = 0; i < SERVICE_NUM_PACKETS; i++)
	{
		packet_t * packet = malloc(sizeof(packet_t));
		memset(packet, 0, sizeof(packet_t));
		stack_push(&packets_free, &packet->packet_list);
	}
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

		if (mainloop_addnewfdtimer(serviceloop, "Service monitor", SERVICE_MONITOR_TIMEOUT, service_monitor, NULL, &e) < 0)
		{
			LOG(LOG_ERR, "Could not create service monitor timer: %s", exception_message(e));
			exception_free(e);
			return false;
		}

		if ((serviceeventfd = mainloop_addnewfdevent(serviceloop, "Service dispatch", 0, service_dispatch, NULL, &e)) < 0)
		{
			LOG(LOG_ERR, "Could not create service dispatch event: %s", exception_message(e));
			exception_free(e);
			return false;
		}

		if (!kthread_newthread("Service handler", KTH_PRIO_LOW, service_runloop, service_stoploop, serviceloop, &e))
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
