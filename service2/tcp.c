#include <unistd.h>
#include <sys/socket.h>
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


static void tcp_send(client_t * client, buffer_t data)
{

}

static bool tcp_check(client_t * client)
{

	return true;
}

static void tcp_destroy(client_t * client)
{

}

static bool tcp_newdata(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{
	client_t * client = userdata;
	tcpclient_t * tcpclient = client_data(client);

	// Update buffer
	{
		ssize_t bytesread = recv(fd, tcpclient->buffer, SC_BUFFERSIZE - tcpclient->size, 0);
		if (bytesread < 0)
		{
			// Abnormal disconnect
			LOG(LOG_INFO, "Abnormal tcp service client disconnect %s", tcpclient->ip.string);
			stream_freeclient(client);
			return false;
		}
		else if (bytesread == 0)
		{
			// Normal disconnect
			LOG(LOG_DEBUG, "Normal tcp service client disconnect %s", tcpclient->ip.string);
			stream_freeclient(client);
			return false;
		}

		tcpclient->size += bytesread;
	}

	// Parse the control codes out
	{
		ssize_t size = 0;

		do
		{
			size = client_control(client, tcpclient->buffer, tcpclient->size);

			if (size < 0)
			{
				// -1 means free the client
				stream_freeclient(client);
				break;
			}

			if (size > 0)
			{
				// size is the number of control bytes consumed
				memmove(tcpclient->buffer, &tcpclient->buffer[size], tcpclient->size - size);
				tcpclient->size -= size;
			}

		} while (size > 0);
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
		LOG(LOG_WARN, "Could not accept new tcp service client socket: %s", strerror(errno));
		return true;
	}

	client_t * client = NULL;

	// Get new client object
	{
		exception_t * e = NULL;
		client = stream_newclient(stream, &e);
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

	stream_t * stream = service_newstream("tcp", sizeof(tcpclient_t), tcp_send, tcp_check, tcp_destroy, err);
	if (stream == NULL || exception_check(err))
	{
		return false;
	}

	int tcpfd = tcp_server(tcp_port, err);
	if (tcpfd < 0 || exception_check(err))
	{
		return false;
	}

	if (!mainloop_addfdwatch(stream_mainloop(stream), tcpfd, FD_READ, tcp_newclient, stream, err))
	{
		return false;
	}

	return true;
}
