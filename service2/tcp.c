#include <unistd.h>
#include <sys/socket.h>
#include <linux/tcp.h>
#include <netinet/in.h>

#include <aul/string.h>
#include <aul/net.h>

#include <kernel.h>

#include <service.h>


int tcp_port = DEFAULT_TCP_PORT;
double tcp_timeout = DEFAULT_NET_TIMEOUT;

module_config(tcp_port, 'i', "TCP port to listen for service requests and send service data on");
module_config(tcp_port, 'd', "TCP timeout (in seconds) with no heartbeat before client is disconnect");

typedef struct
{
	int fd;
} tcpstream_t;

typedef struct
{
	int fd;
	string_t ip;

	uint8_t buffer[SC_BUFFERSIZE];
	size_t size;
} tcpclient_t;


static inline string_t addr2string(uint32_t ip)
{
	unsigned char a4 = (ip & 0xFF000000) >> 24;
	unsigned char a3 = (ip & 0x00FF0000) >> 16;
	unsigned char a2 = (ip & 0x0000FF00) >> 8;
	unsigned char a1 = ip & 0xFF;
	return string_new("%d.%d.%d.%d", a1, a2, a3, a4);
}

static void tcp_streamdestroy(stream_t * stream)
{
	tcpstream_t * tcpstream = stream_data(stream);
	close(tcpstream->fd);
}

static bool tcp_clientsend(client_t * client, int64_t microtimestamp, const buffer_t * data)
{
	tcpclient_t * tcpclient = client_data(client);

	// Write the header
	{
		int64_t ts = microtimestamp;
		uint32_t s = buffer_size(data);

		#define a(x)	((uint8_t *)&x)
		const uint8_t header[] = {
			SC_DATA,																			// Control byte
			a(ts)[0], a(ts)[1], a(ts)[2], a(ts)[3], a(ts)[4], a(ts)[5], a(ts)[6], a(ts)[7],		// 64 bit microsecond timestamp
			a(s)[0], a(s)[1], a(s)[2], a(s)[3]													// Size of data payload to follow
		};

		ssize_t wrote = write(tcpclient->fd, header, sizeof(header));
		if (wrote != sizeof(header))
		{
			// Unable to complete write, return false to indicate error
			return false;
		}
	}

	// Write the payload
	{
		size_t size = buffer_size(data);
		if (buffer_send(data, tcpclient->fd, 0, size) != size)
		{
			// Could not send all data
			return false;
		}
	}
	return true;
}

static void tcp_clientheartbeat(client_t * client)
{
	tcpclient_t * tcpclient = client_data(client);

	// Write the one-byte heartbeat code
	{
		static const uint8_t data = SC_HEARTBEAT;
		write(tcpclient->fd, &data, sizeof(uint8_t));
	}
}

static bool tcp_clientcheck(client_t * client)
{
	// Return false if client hasn't heartbeat'd since more than DEFAULT_NET_TIMEOUT ago
	return (client_lastheartbeat(client) + DEFAULT_NET_TIMEOUT) > kernel_timestamp();
}

static void tcp_clientdestroy(client_t * client)
{
	tcpclient_t * tcpclient = client_data(client);
	close(tcpclient->fd);
}

static bool tcp_newdata(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{
	client_t * client = userdata;
	tcpclient_t * tcpclient = client_data(client);

	// Update buffer
	{
		ssize_t bytesread = recv(fd, &tcpclient->buffer[tcpclient->size], SC_BUFFERSIZE - tcpclient->size, 0);
		if (bytesread <= 0)
		{
			// Normal disconnect
			LOG(LOG_DEBUG, "Abnormal tcp service client disconnect %s", tcpclient->ip.string);
			client_destroy(client);
			return false;
		}

		tcpclient->size += bytesread;
	}

	// Parse the control codes out
	{
		ssize_t size = 0;

		do
		{
			mutex_lock(client_lock(client));
			{
				size = clienthelper_control(client, tcpclient->buffer, tcpclient->size);
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
				memmove(tcpclient->buffer, &tcpclient->buffer[size], tcpclient->size - size);
				tcpclient->size -= size;
			}

		} while (size > 0 && tcpclient->size > 0);
	}

	return true;
}

static bool tcp_newclient(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	stream_t * stream = userdata;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

	socklen_t socklen = sizeof(addr);
	int sock = accept(fd, (struct sockaddr *)&addr, &socklen);
	if (sock < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			LOG(LOG_WARN, "Could not accept new tcp service client socket: %s", strerror(errno));
		}

		return true;
	}

	// Set the TCP_NODELAY flag on the socket
	{
		int flag = 1;
		if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int)) < 0)
		{
			// Do nothing but warn
			LOG(LOG_WARN, "Could not set tcp_nodelay on service client tcp socket: %s", strerror(errno));
		}
	}

	client_t * client = NULL;

	// Get new client object
	{
		exception_t * e = NULL;
		client = client_new(stream, &e);
		if (client == NULL || exception_check(&e))
		{
			LOG(LOG_WARN, "Service client register error: %s", exception_message(e));
			exception_free(e);

			close(sock);
			return true;
		}
	}

	// Init the tcpstream
	tcpclient_t * tcpclient = client_data(client);
	memset(tcpclient, 0, sizeof(tcpclient_t));
	tcpclient->fd = sock;
	tcpclient->ip = addr2string(addr.sin_addr.s_addr);

	LOG(LOG_DEBUG, "New tcp service client from %s", tcpclient->ip.string);

	// Add it to the mainloop
	{
		exception_t * e = NULL;
		if (!mainloop_addfdwatch(stream_mainloop(stream), sock, FD_READ, tcp_newdata, client, &e))
		{
			LOG(LOG_WARN, "Could not add tcp client to mainloop: %s", exception_message(e));
			exception_free(e);

			close(sock);
			return true;
		}
	}

	return true;
}

bool tcp_init(exception_t ** err)
{
	if (tcp_port <= 0)
	{
		LOG(LOG_INFO, "Service tcp stream disabled (invalid tcp port configuration)");
		return true;
	}

	stream_t * stream = stream_new("tcp", sizeof(tcpstream_t), tcp_streamdestroy, sizeof(tcpclient_t), tcp_clientsend, tcp_clientheartbeat, tcp_clientcheck, tcp_clientdestroy, err);
	if (stream == NULL || exception_check(err))
	{
		return false;
	}

	tcpstream_t * tcpstream = stream_data(stream);
	tcpstream->fd = tcp_server(tcp_port, err);
	if (tcpstream->fd < 0 || exception_check(err))
	{
		stream_destroy(stream);
		return false;
	}

	if (!mainloop_addfdwatch(stream_mainloop(stream), tcpstream->fd, FD_READ, tcp_newclient, stream, err))
	{
		stream_destroy(stream);
		return false;
	}

	return true;
}
