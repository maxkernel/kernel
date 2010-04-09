#include <string.h>
#include <gnet.h>

#include <kernel.h>
#include <buffer.h>


DEF_BLOCK(jpeghttp, jpeghttp_new, "i");
BLK_ONUPDATE(jpeghttp, jpeghttp_update);
BLK_INPUT(jpeghttp, frame, "x");


/* Adapted from MJPG-Streamer   Thanks guys! */
#define BOUNDARY			"myboundry"
#define STDHEADER			"HTTP/1.0 200 OK\r\n" \
								"Connection: close\r\n" \
								"Server: MaxKernel-v" VERSION "\r\n" \
								"Cache-Control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0\r\n" \
								"Pragma: no-cache\r\n" \
								"Expires: Sat, 1 Jan 2000 00:00:01 GMT\r\n" \
								"Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY "\r\n" \
								"\r\n" \
								"--" BOUNDARY "\r\n"
#define PREFRAMEHEADER		"Content-Type: image/jpeg\r\n" \
								"Content-Length: %d\r\n" \
								"\r\n"
#define POSTFRAMEHEADER		"\r\n--" BOUNDARY "\r\n"



typedef struct
{
	const char * class_name;
	char * obj_name;
	unsigned int obj_id;
	info_f info;
	destructor_f destructor;

	unsigned int port;
	void * socket;
	GSList * clients;
	mutex_t * mutex;
} server_t;

typedef struct
{
	server_t * parent;
	void * socket;
	GIOChannel * gio;
} client_t;


static char * jpeghttp_info(void * object)
{
	char * str = "[SERVER INFO PLACEHOLDER]";
	return strdup(str);
}

static void jpeghttp_destroy(void * object)
{
	server_t * server = object;

	mutex_lock(server->mutex);

	GSList * next = server->clients;
	while (next != NULL)
	{
		client_t * client = next->data;
		if (client != NULL)
		{
			gnet_tcp_socket_delete(client->socket);
		}

		next = next->next;
	}
	gnet_tcp_socket_delete(server->socket);

	mutex_unlock(server->mutex);
	//mutex_free(server->mutex);
}

static boolean jpeghttp_clientdata(GIOChannel * gio, GIOCondition condition, void * data)
{
	client_t * client = data;
	server_t * server = client->parent;

	//ignore all input (for now)
	size_t read = 0;
	char buf[50];

	g_io_channel_read(gio, buf, sizeof(buf), &read);

	if (read == 0)
	{
		//user has disconnected

		//remove from list
		mutex_lock(server->mutex);

		GSList * item = g_slist_find(server->clients, client);
		if (item != NULL)
		{
			item->data = NULL;
			g_free(client);
		}

		mutex_unlock(server->mutex);

		return FALSE;
	}

	return TRUE;
}

static boolean jpeghttp_newclient(GIOChannel * gio, GIOCondition condition, void * data)
{
	server_t * server = data;

	LOG(LOG_DEBUG, "New videoserver client.");

	GTcpSocket * sock = gnet_tcp_socket_server_accept_nonblock(server->socket);
	if (sock == NULL)
	{
		//sock error
		LOG(LOG_WARN, "Could not read new network socket");

		server->socket = NULL;
		return FALSE;
	}

	GIOChannel * sockchan = gnet_tcp_socket_get_io_channel(sock);

	//create client
	client_t * client = g_malloc0(sizeof(client_t));
	client->socket = sock;
	client->parent = server;
	client->gio = sockchan;

	//add client to list
	mutex_lock(server->mutex);
	server->clients = g_slist_append(server->clients, client);
	mutex_unlock(server->mutex);

	g_io_add_watch(sockchan, G_IO_IN, jpeghttp_clientdata, client);

	//now send standard header
	size_t wrote = 0;
	g_io_channel_write_chars(sockchan, STDHEADER, strlen(STDHEADER), &wrote, NULL);
	if (wrote != strlen(STDHEADER))
	{
		char * ip = GETIP(sock);
		LOG(LOG_WARN, "Could not write complete video-streamer header to client %s", ip);
		g_free(ip);
	}
	g_io_channel_flush(sockchan, NULL);

	return TRUE;
}

void * jpeghttp_new(int port)
{
	String name = string_new("HTTP (port %d)", port);

	server_t * server = kobj_new("JpegHttpStream", string_copy(&name), jpeghttp_info, jpeghttp_destroy, sizeof(server_t));
	server->port = port;
	server->socket = gnet_tcp_socket_server_new_with_port(port);
	server->mutex = mutex_new(M_RECURSIVE);

	if (server->socket == NULL)
	{
		LOG(LOG_WARN, "Error creating network server socket on port %d. Will not except any videoserver clients", port);
	}
	else
	{
		GIOChannel * serverchan = gnet_tcp_socket_get_io_channel(server->socket);
		g_io_add_watch(serverchan, G_IO_IN, jpeghttp_newclient, server);
	}

	return server;
}

void jpeghttp_update(void * data)
{
	server_t * server = data;
	if (server == NULL || ISNULL(jpeg_frame) || g_slist_length(server->clients) == 0)
	{
		LOG(LOG_INFO, "HTTP NO DATA %d", ISNULL(jpeg_frame));
		return;
	}

	LOG(LOG_INFO, "HTTP DATA");

	buffer_t jpeg_frame = INPUTT(buffer_t, jpeg_frame);

	GString * header = g_string_new("");
	g_string_printf(header, PREFRAMEHEADER, buffer_datasize(jpeg_frame));
	size_t s1 = header->len, s2 = buffer_datasize(jpeg_frame), s3 = strlen(POSTFRAMEHEADER);

	mutex_lock(server->mutex);

	GSList * next = server->clients;
	while (next != NULL)
	{
		client_t * client = next->data;
		if (client != NULL)
		{
			size_t w1 = 0, w2 = 0, w3 = 0;
			g_io_channel_write_chars(client->gio, header->str, s1, &w1, NULL);
			g_io_channel_write_chars(client->gio, buffer_data(jpeg_frame), s2, &w2, NULL);
			g_io_channel_write_chars(client->gio, POSTFRAMEHEADER, s3, &w3, NULL);
			g_io_channel_flush(client->gio, NULL);

			if (w1 != s1 || w2 != s2 || w3 != s3)
			{
				char * ip = GETIP(server->socket);
				LOG(LOG_WARN, "Could not write complete video frame to %s", ip);
				g_free(ip);
			}
		}

		next = next->next;
	}

	mutex_unlock(server->mutex);

	g_string_free(header, TRUE);
}

