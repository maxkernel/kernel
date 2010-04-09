

#include <aul/common.h>
#include <aul/contrib/list.h>
#include <aul/mutex.h>

#include <kernel.h>
#include <service.h>
#include "internal.h"


typedef struct
{
	stream_t * stream;
	packet_t packet;

	struct list_head list;
} packetbuf_t;


static boolean stop = FALSE;
static packetbuf_t packets[SERVICE_PACKETS_MAX];

static struct list_head packets_queue;
static struct list_head packets_free;

static mutex_t listlock;
static cond_t barrier;

void send_init()
{
	ZERO(packets);

	mutex_init(&listlock, M_RECURSIVE);
	cond_init(&barrier);

	INIT_LIST_HEAD(&packets_queue);
	INIT_LIST_HEAD(&packets_free);

	size_t i=0;
	for (; i<SERVICE_PACKETS_MAX; i++)
	{
		list_add(&packets[i].list, &packets_free);
	}
}

void send_data(service_h service_handle, client_h client_handle, stream_t * stream, uint64_t timestamp_us, const void * data, size_t length)
{
	size_t numpackets = length / SERVICE_FRAGSIZE;
	if ((length % SERVICE_FRAGSIZE) > 0)
	{
		numpackets += 1;
	}

	size_t onpacket;

	struct list_head packets;
	INIT_LIST_HEAD(&packets);

	mutex_lock(&listlock);
	{
		for (onpacket = 0; onpacket < numpackets; onpacket++)
		{
			if (packets_free.next != &packets_free)
			{
				packetbuf_t * data = list_entry(packets_free.next, packetbuf_t, list);
				list_del(packets_free.next);
				list_add(&data->list, &packets);
			}
			else
			{
				break;
			}
		}
	}
	mutex_unlock(&listlock);


	if (onpacket < numpackets)
	{
		LOG(LOG_WARN, "Could not send data service data, out of free packet buffers!");

		//put the buffers pack on the free list
		mutex_lock(&listlock);
		{
			struct list_head * pos, * n;
			list_for_each_safe(pos, n, &packets)
			{
				packetbuf_t * item = list_entry(pos, packetbuf_t, list);
				list_del(pos);
				list_add(&item->list, &packets_free);
			}
		}
		mutex_unlock(&listlock);

		return;
	}

	//put data into packet buffers
	{
		uint16_t fragnum = 0;
		size_t index = 0;

		struct list_head * pos, * n;
		list_for_each_safe(pos, n, &packets)
		{
			packetbuf_t * buf = list_entry(pos, packetbuf_t, list);
			buf->stream = stream;

			//assemble header
			packet_t * packet = &buf->packet;
			packet->data.header.sync = PACKET_SYNC;
			strncpy(packet->data.header.service_handle, service_handle, HANDLE_MAXLEN);
			strncpy(packet->data.header.client_handle, client_handle, HANDLE_MAXLEN);
			packet->data.header.frame_length = length;
			packet->data.header.frag_size = SERVICE_FRAGSIZE;
			packet->data.header.frag_num = fragnum++;
			packet->data.header.timestamp = timestamp_us;

			size_t payloadsize = MIN(length - index, SERVICE_FRAGSIZE);
			memcpy(packet->data.packet.payload, ((char *)data) + index, payloadsize);
			packet->size = payloadsize + HEADER_LENGTH;
			index += payloadsize;

			list_del(pos);
			mutex_lock(&listlock);
			{
				list_add_tail(pos, &packets_queue);
				cond_signal(&barrier);
			}
			mutex_unlock(&listlock);
		}
	}
}

void send_startthread(void * userdata)
{
	while (!stop)
	{
		mutex_lock(&listlock);
		cond_wait(&barrier, &listlock, 0);
		mutex_unlock(&listlock);

		if (!stop)
		{
			packetbuf_t * data = NULL;

			do
			{
				mutex_lock(&listlock);
				{
					if (packets_queue.next != &packets_queue)
					{
						data = list_entry(packets_queue.next, packetbuf_t, list);
						list_del(packets_queue.next);
					}
					else
					{
						data = NULL;
					}
				}
				mutex_unlock(&listlock);

				if (data != NULL)
				{
					//LOG(LOG_INFO, "SEND FRAG #%d", data->packet.data.header.frag_num);
					data->stream->send(data->stream, &data->packet);

					//put data on packet_free list
					mutex_lock(&listlock);
					{
						list_add(&data->list, &packets_free);
					}
					mutex_unlock(&listlock);
				}
			} while (data != NULL);
		}
	}
}

void send_stopthread(void * userdata)
{
	stop = TRUE;

	mutex_lock(&listlock);
	{
		cond_broadcast(&barrier);
	}
	mutex_unlock(&listlock);
}