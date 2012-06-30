#include <unistd.h>
#include <sys/socket.h>
#include <linux/tcp.h>
#include <netinet/in.h>

#include <aul/string.h>
#include <aul/net.h>

#include <kernel.h>

#include <service.h>


int udp_port = DEFAULT_UDP_PORT;
double udp_timeout = DEFAULT_NET_TIMEOUT;

module_config(udp_port, 'i', "UDP port to listen for service requests and send service data on");
module_config(udp_timeout, 'd', "UDP timeout (in seconds) with no heartbeat before client is disconnect");

typedef struct
{
	fdwatcher_t watcher;
	mutex_t lock;
	list_t clients;
} udpstream_t;

typedef struct
{
	client_t * client;
	list_t stream_list;

	in_addr_t ip;
	in_port_t port;
} udpclient_t;


static inline string_t addr2string(in_addr_t ip)
{
	unsigned char a4 = (ip & 0xFF000000) >> 24;
	unsigned char a3 = (ip & 0x00FF0000) >> 16;
	unsigned char a2 = (ip & 0x0000FF00) >> 8;
	unsigned char a1 = ip & 0xFF;
	return string_new("%d.%d.%d.%d", a1, a2, a3, a4);
}

static void udp_streamdestroy(stream_t * stream)
{
	udpstream_t * udpstream = stream_data(stream);

	// Destroy the udp watcher
	{
		exception_t * e = NULL;
		if (!mainloop_removewatcher(&udpstream->watcher, &e) || exception_check(&e))
		{
			LOG(LOG_WARN, "Could not remove udp stream watcher: %s", exception_message(e));
			exception_free(e);
		}
	}

	mutex_destroy(&udpstream->lock);
}

static bool udp_clientsend(client_t * client, int64_t microtimestamp, const buffer_t * data)
{
	udpclient_t * udpclient = client_data(client);
	udpstream_t * udpstream = stream_data(client_stream(client));

	LOG(LOG_INFO, "SEND TO UDP GS");
	unused(udpclient);
	unused(udpstream);

	return true;
}

static void udp_clientheartbeat(client_t * client)
{
	udpclient_t * udpclient = client_data(client);
	udpstream_t * udpstream = stream_data(client_stream(client));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = udpclient->port;
	addr.sin_addr.s_addr = udpclient->ip;

	static const uint8_t data = SC_HEARTBEAT;
	sendto(watcher_fd(&udpstream->watcher), &data, sizeof(uint8_t), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
}

static bool udp_clientcheck(client_t * client)
{
	// Return false if client hasn't heartbeat'd since more than DEFAULT_NET_TIMEOUT ago
	return (client_lastheartbeat(client) + DEFAULT_NET_TIMEOUT) > kernel_timestamp();
}

static bool udp_newdata(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{
	stream_t * stream = userdata;
	udpstream_t * udpstream = stream_data(stream);


	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	socklen_t addrlen = sizeof(addr);

	uint8_t buffer[SC_BUFFERSIZE];
	ssize_t read = recvfrom(fd, buffer, SC_BUFFERSIZE, 0, (struct sockaddr *)&addr, &addrlen);
	if (read < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			LOG(LOG_WARN, "Could not read data from udp service client socket: %s", strerror(errno));
		}

		return true;
	}

	in_addr_t ip = addr.sin_addr.s_addr;
	in_port_t port = addr.sin_port;

	// Look up the client
	client_t * client = NULL;
	udpclient_t * udpclient = NULL;
	mutex_lock(&udpstream->lock);
	{
		list_t * pos = NULL;
		list_foreach(pos, &udpstream->clients)
		{
			udpclient_t * testudpclient = list_entry(pos, udpclient_t, stream_list);
			if (testudpclient->ip == ip && testudpclient->port == port)
			{
				udpclient = testudpclient;
				client = udpclient->client;
				break;
			}
		}
	}
	mutex_unlock(&udpstream->lock);

	if (udpclient == NULL)
	{
		// New client
		exception_t * e = NULL;
		client = client_new(stream, &e);
		if (client == NULL || exception_check(&e))
		{
			LOG(LOG_WARN, "Service udp client register error: %s", exception_message(e));
			exception_free(e);

			return true;
		}

		// Initialize the udpclient
		udpclient = client_data(client);
		udpclient->client = client;
		udpclient->ip = ip;
		udpclient->port = port;

		// Add the udpclient to the stream list
		mutex_lock(&udpstream->lock);
		{
			list_add(&udpstream->clients, &udpclient->stream_list);
		}
		mutex_unlock(&udpstream->lock);
	}


	ssize_t size = client_control(client, buffer, read);
	if (size <= 0)
	{
		// Close client or did not receive complete packet
		client_destroy(client);
	}

	//uint16_t clientid =
	// TODO - finish me!

	/*
	// Update buffer
	{
		ssize_t bytesread = recv(fd, &udpclient->buffer[udpclient->size], SC_BUFFERSIZE - udpclient->size, 0);
		if (bytesread <= 0)
		{
			// Normal disconnect
			LOG(LOG_DEBUG, "Abnormal tcp service client disconnect %s", addr2string(udpclient->ip).string);
			client_destroy(client);
			return false;
		}

		udpclient->size += bytesread;
	}

	// Parse the control codes out
	{
		ssize_t size = 0;

		do
		{
			mutex_lock(client_lock(client));
			{
				size = clienthelper_control(client, udpclient->buffer, udpclient->size);
			}
			mutex_unlock(client_lock(client));

			if (size < 0)
			{
				// -1 means free the client
				LOG(LOG_INFO, "Destroy because of control");
				client_destroy(client);
				return false;
			}

			if (size > 0)
			{
				// size is the number of control bytes consumed
				memmove(udpclient->buffer, &udpclient->buffer[size], udpclient->size - size);
				udpclient->size -= size;
			}

		} while (size > 0 && udpclient->size > 0);
	}
	*/

	return true;
}

bool udp_init(exception_t ** err)
{
	if (udp_port <= 0)
	{
		LOG(LOG_INFO, "Service udp stream disabled (invalid udp port configuration)");
		return true;
	}

	int fd = udp_server(udp_port, err);
	if (fd < 0 || exception_check(err))
	{
		return false;
	}

	stream_t * stream = stream_new("udp", sizeof(udpstream_t), udp_streamdestroy, sizeof(udpclient_t), udp_clientsend, udp_clientheartbeat, udp_clientcheck, NULL, err);
	if (stream == NULL || exception_check(err))
	{
		return false;
	}

	udpstream_t * udpstream = stream_data(stream);
	watcher_newfd(&udpstream->watcher, fd, FD_READ, udp_newdata, stream);
	mutex_init(&udpstream->lock, M_RECURSIVE);
	list_init(&udpstream->clients);

	if (!mainloop_addwatcher(stream_mainloop(stream), &udpstream->watcher, err) || exception_check(err))
	{
		stream_destroy(stream);
		return false;
	}

	return true;
}
