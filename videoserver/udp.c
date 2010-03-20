#include <string.h>
#include <gnet.h>

#include <kernel.h>
#include <buffer.h>
#include "videoserver.h"


DEF_BLOCK(udp, udp_new, "i");
BLK_ONUPDATE(udp, udp_update);
BLK_INPUT(udp, jpeg_frame, "x");


#define FRAG_SIZE	10000

static char * udp_info(void * object)
{
	char * str = "[SERVER INFO PLACEHOLDER]";
	return strdup(str);
}

static void udp_destroy(void * object)
{
	server_t * server = object;
	gnet_udp_socket_delete(server->socket);

	g_mutex_lock(server->mutex);
	g_mutex_unlock(server->mutex);
	g_mutex_free(server->mutex);
}


void * udp_new(int port)
{
	GString * name = g_string_new("");
	g_string_printf(name, "UDP (port %d)", port);

	server_t * server = kobj_new("VideoStream", g_string_free(name, FALSE), udp_info, udp_destroy, sizeof(server_t));
	server->port = port;
	server->socket = gnet_udp_socket_new();
	server->mutex = g_mutex_new();

	server->clients = g_slist_append(server->clients, gnet_inetaddr_new("10.0.9.11", server->port));
	//10.0.9.11

	return server;
}

void udp_update(void * data)
{
	server_t * server = data;
	if (server == NULL || ISNULL(jpeg_frame) || g_slist_length(server->clients) == 0)
	{
		return;
	}

	buffer_t jpeg_frame = INPUTT(buffer_t, jpeg_frame);

	char buf[FRAG_SIZE + 24];
	memset(buf, 0, 24);													//zero out header
	*(uint32_t *)(buf+8) = buffer_datasize(jpeg_frame);		//frame length
	*(uint16_t *)(buf+12) = FRAG_SIZE;								//fragmentation size
	*(uint64_t *)(buf+16) = kernel_timestamp();				//timestamp

	g_mutex_lock(server->mutex);

	size_t i=0;
	for (; i<buffer_datasize(jpeg_frame); i += FRAG_SIZE)
	{
		size_t len = MIN(FRAG_SIZE, buffer_datasize(jpeg_frame)-i);
		memcpy(buf+24, buffer_data(jpeg_frame)+i, len);

		GSList * next = server->clients;
		while (next != NULL)
		{
			GInetAddr * client = next->data;
			if (client != NULL)
			{
				gnet_udp_socket_send(server->socket, buf, len + 24, client);
			}

			next = next->next;
		}

		//increment fragmentation number
		*(uint16_t *)(buf+14) += 1;
	}

	g_mutex_unlock(server->mutex);
}
