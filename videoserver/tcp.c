#include <string.h>
#include <gnet.h>

#include <kernel.h>
#include <buffer.h>
#include "videoserver.h"

DEF_BLOCK(tcp, tcp_new, "i");
BLK_ONUPDATE(tcp, tcp_update);
BLK_INPUT(tcp, jpeg_frame, "x");


#define FRAG_SIZE	10000


static char * tcp_info(void * object)
{
	char * str = "[TCP SERVER INFO PLACEHOLDER]";
	return strdup(str);
}

static void tcp_destroy(void * object)
{
	server_t * server = object;

	g_mutex_lock(server->mutex);

	GSList * next = server->clients;
	while (next != NULL)
	{
		client_t * client = next->data;
		if (client != NULL)
		{
			g_io_channel_close(client->gio);
			gnet_tcp_socket_delete(client->socket);
		}

		next = next->next;
	}
	gnet_tcp_socket_delete(server->socket);

	g_mutex_unlock(server->mutex);
	g_mutex_free(server->mutex);
}

static boolean tcp_newclient(GIOChannel * gio, GIOCondition condition, void * data)
{
	server_t * server = data;

	LOG(LOG_DEBUG, "New videoserver TCP client.");

	GTcpSocket * sock = gnet_tcp_socket_server_accept_nonblock(server->socket);
	if (sock == NULL)
	{
		//sock error
		LOG(LOG_WARN, "Could not read new network socket");

		server->socket = NULL;
		return FALSE;
	}

	GError * err = NULL;

	GIOChannel * sockchan = gnet_tcp_socket_get_io_channel(sock);
	g_io_channel_set_flags(sockchan, G_IO_FLAG_NONBLOCK, &err);

	if (err != NULL)
	{
		LOG(LOG_ERR, "Could not set IOChannel flags on client socket: %s", err->message);
		g_error_free(err);
	}

	//create client
	client_t * client = g_malloc0(sizeof(client_t));
	client->socket = sock;
	client->parent = server;
	client->gio = sockchan;

	//add client to list
	g_mutex_lock(server->mutex);
	server->clients = g_slist_append(server->clients, client);
	g_mutex_unlock(server->mutex);

	return TRUE;
}

void * tcp_new(int port)
{
	GString * name = g_string_new("");
	g_string_printf(name, "TCP (port %d)", port);

	server_t * server = kobj_new("VideoStream", g_string_free(name, FALSE), tcp_info, tcp_destroy, sizeof(server_t));
	server->port = port;
	server->socket = gnet_tcp_socket_server_new_with_port(port);
	server->mutex = g_mutex_new();

	if (server->socket == NULL)
	{
		LOG(LOG_WARN, "Error creating network server socket on port %d. Will not except any videoserver clients", port);
	}
	else
	{
		GIOChannel * serverchan = gnet_tcp_socket_get_io_channel(server->socket);
		g_io_add_watch(serverchan, G_IO_IN, tcp_newclient, server);
	}

	return server;
}

void tcp_update(void * data)
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
		len += 24;

		GSList * next = server->clients;
		while (next != NULL)
		{
			client_t * client = next->data;
			if (client != NULL)
			{
				size_t wrote = 0;

				while (wrote < len)
				{
					size_t w = 0;
					if (g_io_channel_write_chars(client->gio, buf+wrote, len-wrote, &w, NULL) == G_IO_STATUS_ERROR)
					{
						LOG(LOG_DEBUG, "TCP Video Client disconnected");
						g_io_channel_close(client->gio);
						gnet_tcp_socket_delete(client->socket);

						next->data = NULL;

						break;
					}

					g_io_channel_flush(client->gio, NULL);
					wrote += w;
				}


				//LOG(LOG_INFO, "WROTE %d to client (jpeg size = %d)", w, buffer_datasize(jpeg_frame));
/*
				if (w != len)
				{
					LOG(LOG_WARN, "Could not write complete video frame");
					g_io_channel_close(client->gio);
					gnet_tcp_socket_delete(client->socket);
					next->data = NULL;
				}
*/
			}

			next = next->next;
		}

		//increment fragment #
		*(uint16_t *)(buf+14) += 1;
	}

	g_mutex_unlock(server->mutex);
}
