#include <stdio.h>

#include <aul/mainloop.h>

#include <kernel.h>
#include <service.h>
#include "internal.h"


mutex_t service_lock;
mainloop_t * serviceloop = NULL;
char streamconfig_cache[200];

GHashTable * service_table = NULL;
GHashTable * client_table = NULL;
GHashTable * stream_table = NULL;

client_t clients[SERVICE_CLIENTS_MAX];


void service_freestream(stream_t * stream)
{
	if (stream == NULL)
	{
		return;
	}

	//remove stream from lookup table
	mutex_lock(&service_lock);
	{
		g_hash_table_remove(stream_table, stream->handle);
	}
	mutex_unlock(&service_lock);

	//call destroy on the stream
	if (stream->destroy != NULL)
	{
		stream->destroy(stream);
	}

	//remove it from all services
	mutex_lock(&service_lock);
	{
		GHashTableIter itr;
		g_hash_table_iter_init(&itr, service_table);

		service_t * service = NULL;
		while (g_hash_table_iter_next(&itr, NULL, (void **)&service))
		{
			mutex_lock(&service->lock);
			{
				size_t i = 0;
				bool found = false;

				for (; i<STREAMS_PER_SERVICE; i++)
				{
					if (service->streams[i] == stream)
					{
						service->streams[i] = NULL;
						found = true;
						break;
					}
				}

				//unregister stream from service
				if (found && service->disconnect != NULL)
				{
					service->disconnect(service->handle, stream->handle);
				}
			}
			mutex_unlock(&service->lock);
		}
	}
	mutex_unlock(&service_lock);

	//remove it from client list
	client_t * client = stream->client;
	if (client != NULL)
	{
		mutex_lock(&client->lock);
		{
			client->streams[stream->protocol] = NULL;
		}
		mutex_unlock(&client->lock);
	}

	stream->inuse = false;
}

void * service_newstream(void * array, protocol_t protocol, psend_f send, pdestroy_f destroy, size_t size)
{
	char * a = array;
	stream_t * pdata = NULL;

	mutex_lock(&service_lock);
	{
		size_t i=0;
		for (; i<SERVICE_CLIENTS_MAX; i++)
		{
			stream_t * item = (stream_t *)a;

			if (!item->inuse)
			{
				PZERO(item, size);
				item->inuse = true;
				item->protocol = protocol;
				item->send = send;
				item->destroy = destroy;

				generateid(item->handle, stream_table);
				g_hash_table_insert(stream_table, item->handle, item);

				pdata = item;
				break;
			}

			a += size;
		}
	}
	mutex_unlock(&service_lock);

	if (pdata == NULL)
	{
		LOG(LOG_WARN, "Cannot accept new service client, SERVICE_CLIENTS_MAX (%d) exceeded.", SERVICE_CLIENTS_MAX);
	}

	return pdata;
}

client_t * service_getclient(char * client_handle)
{
	client_t * client = NULL;

	mutex_lock(&service_lock);
	{
		client = g_hash_table_lookup(client_table, client_handle);
	}
	mutex_unlock(&service_lock);

	if (client != NULL)
	{
		return client;
	}

	//client doesn't exist yet
	mutex_lock(&service_lock);
	{
		size_t i=0;
		for (; i<SERVICE_CLIENTS_MAX; i++)
		{
			if (!clients[i].inuse)
			{
				ZERO(clients[i]);
				clients[i].timeout_us = kernel_timestamp();
				clients[i].inuse = true;
				client = &clients[i];
				break;
			}
		}
	}
	mutex_unlock(&service_lock);

	if (client == NULL)
	{
		LOG(LOG_WARN, "Cannot accept new service client, SERVICE_CLIENTS_MAX (%d) exceeded.", SERVICE_CLIENTS_MAX);
		return NULL;
	}

	generateid(client->handle, client_table);
	mutex_init(&client->lock, M_RECURSIVE);

	mutex_lock(&service_lock);
	{
		g_hash_table_insert(client_table, client->handle, client);
	}
	mutex_unlock(&service_lock);

	LOG(LOG_DEBUG, "Created service client (%s)", client->handle);
	return client;
}


static bool service_checktimeout(mainloop_t * loop, uint64_t nanoseconds, void * userdata)
{
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

	return true;
}

static void service_runloop()
{
	mainloop_run(serviceloop);
}

static void service_stoploop()
{
	mainloop_stop(serviceloop);
}

const char * service_getstreamconfig()
{
	string_t tcp = tcp_streamconfig();
	string_t udp = udp_streamconfig();
	snprintf(streamconfig_cache, sizeof(streamconfig_cache), "%s%s", tcp.string, udp.string);
	return streamconfig_cache;
}

void module_preactivate()
{
	PZERO(clients, sizeof(clients));

	mutex_init(&service_lock, M_RECURSIVE);
	service_table = g_hash_table_new(g_str_hash, g_str_equal);
	client_table = g_hash_table_new(g_str_hash, g_str_equal);
	stream_table = g_hash_table_new(g_str_hash, g_str_equal);
}

bool module_init()
{
	LOG(LOG_DEBUG, "Initializing Service subsystem");

	//initialize default services
	service_default_init();

	serviceloop = malloc0(sizeof(mainloop_t));
	mainloop_new("Service network loop", serviceloop);
	mainloop_addtimer(serviceloop, "Service timeout checker", SERVICE_TIMEOUT_US * MILLIS_PER_SECOND, service_checktimeout, NULL);
	kthread_newthread("Service server", 5, service_runloop, service_stoploop, NULL);

	send_init();
	{
		size_t i=0;
		for (; i<SERVICE_SENDER_THREADS; i++)
		{
			kthread_newthread("Service sender", KTH_PRIO_MEDIUM, send_startthread, send_stopthread, NULL);
		}
	}

	tcp_init();
	udp_init();

	return true;
}


module_name("Service API Server");
module_version(0,6,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Provides a publish/subscribe API to help modules stream sensors/status/other stuff to external clients");
module_onpreactivate(module_preactivate);
module_oninitialize(module_init);

// TODO - better protocol?
define_syscall(service_getstreamconfig, "s:v", "Get a new-line seperated key=value string of stream configuration options");
