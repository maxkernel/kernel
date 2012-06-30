#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <linux/tcp.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <aul/string.h>
#include <aul/net.h>

#include <kernel.h>

#include <service.h>


int tcp_port = DEFAULT_TCP_PORT;
double tcp_timeout = DEFAULT_NET_TIMEOUT;

module_config(tcp_port, 'i', "TCP port to listen for service requests and send service data on");
module_config(tcp_timeout, 'd', "TCP timeout (in seconds) with no heartbeat before client is disconnect");

typedef struct
{
	fdwatcher_t socket;
} tcpstream_t;

typedef struct
{
	fdwatcher_t socket;
	uint32_t ip;

	uint8_t buffer[SC_BUFFERSIZE];
	size_t size;

	buffer_t * payload;
	bufferpos_t payload_pos;
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

	// Remove the watcher
	{
		exception_t * e = NULL;
		if (!mainloop_removewatcher(&tcpstream->socket, &e) || exception_check(&e))
		{
			LOG(LOG_WARN, "Could not remove tcp stream watcher: %s", exception_message(e));
			exception_free(e);
		}
	}
}

static bool tcp_clientsend(client_t * client, int64_t microtimestamp, const buffer_t * data)
{
	tcpclient_t * tcpclient = client_data(client);

	// Check to make sure there isn't a saved buffer still
	{
		if (tcpclient->payload != NULL)
		{
			LOG(LOG_WARN, "Could send complete service data payload to tcp client before new data payload. Dropping client");
			return false;
		}
	}

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

		ssize_t wrote = write(watcher_fd(&tcpclient->socket), header, sizeof(header));
		if (wrote != sizeof(header))
		{
			// Unable to complete write, return false to indicate error
			LOG(LOG_INFO, "SEND HEADER ERROR");
			return false;
		}
	}

	// Write the payload
	{
		size_t size = buffer_size(data);
		ssize_t wrote = buffer_send(data, watcher_fd(&tcpclient->socket), 0, size);
		if (wrote < 0)
		{
			// Some error happened during send
			LOG(LOG_INFO, "SEND BODY ERROR");
			return false;
		}
		else if (wrote != size)
		{
			// Could not send all data, queue up the buffer and try to send more when available
			tcpclient->payload = buffer_dup(data);
			bufferpos_new(&tcpclient->payload_pos, tcpclient->payload, wrote);

			exception_t * e = NULL;
			if (!mainloop_rearmwatcher(&tcpclient->socket, &e) || exception_check(&e))
			{
				LOG(LOG_WARN, "Client tcp socket buffer defer error: %s", exception_message(e));
				exception_free(e);
			}
		}
	}
	return true;
}

static void tcp_clientheartbeat(client_t * client)
{
	tcpclient_t * tcpclient = client_data(client);

	// Write the one-byte heartbeat code
	static const uint8_t data = SC_HEARTBEAT;
	write(watcher_fd(&tcpclient->socket), &data, sizeof(uint8_t));
}

static bool tcp_clientcheck(client_t * client)
{
	// Return false if client hasn't heartbeat'd since more than DEFAULT_NET_TIMEOUT ago
	return !client_locked(client) || (client_lastheartbeat(client) + DEFAULT_NET_TIMEOUT) > kernel_timestamp();
}

static void tcp_clientdestroy(client_t * client)
{
	tcpclient_t * tcpclient = client_data(client);

	// Free the payload if set
	{
		if (tcpclient->payload != NULL)
		{
			buffer_free(tcpclient->payload);
			tcpclient->payload = NULL;
			bufferpos_clear(&tcpclient->payload_pos);
		}
	}

	// Destroy the client tcp watcher
	{
		exception_t * e = NULL;
		if (!mainloop_removewatcher(&tcpclient->socket, &e) || exception_check(&e))
		{
			LOG(LOG_WARN, "Could not remove tcp client watcher: %s", exception_message(e));
			exception_free(e);
		}
	}
}

static bool tcp_newdata(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{
	client_t * client = userdata;
	tcpclient_t * tcpclient = client_data(client);

	if (cond & FD_READ)
	{
		// There is pending data to be read on the socket

		// Update buffer
		{
			ssize_t bytesread = read(fd, &tcpclient->buffer[tcpclient->size], SC_BUFFERSIZE - tcpclient->size);
			if (bytesread <= 0)
			{
				// Abnormal disconnect
				LOG(LOG_DEBUG, "Abnormal tcp service client disconnect %s", addr2string(tcpclient->ip).string);
				client_destroy(client);

				// Return true because we already closed the client in client_detsroy (avoid the warning of double closure)
				return true;
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
					client_destroy(client);

					return true;
				}

				if (size > 0)
				{
					// Size is the number of control bytes consumed
					memmove(tcpclient->buffer, &tcpclient->buffer[size], tcpclient->size - size);
					tcpclient->size -= size;
				}

			} while (size > 0 && tcpclient->size > 0);
		}
	}


	if (cond & FD_WRITE && tcpclient->payload != NULL)
	{
		// The socket is writable, send more payload out

		size_t remaining = bufferpos_remaining(&tcpclient->payload_pos);
		ssize_t sent = bufferpos_send(&tcpclient->payload_pos, fd, remaining);

		if (sent < 0)
		{
			// Error during send, destroy client
			LOG(LOG_WARN, "Could not send tcp service data to client: %s", strerror(errno));
			client_destroy(client);

			return true;
		}

		if (sent == remaining)
		{
			// Sent all data, destroy the payload buffer
			buffer_free(tcpclient->payload);
			tcpclient->payload = NULL;
			bufferpos_clear(&tcpclient->payload_pos);
		}
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

	/*
	// Set socket type-of-service
	{
		int tos = IPTOS_THROUGHPUT;
		if (setsockopt(sock, IPPROTO_IP, IP_TOS, &tos, sizeof(int)) < 0)
		{
			// Do nothing but warn
			LOG(LOG_WARN, "Could not set type-of-service of service client tcp socket: %s", strerror(errno));

			close(sock);
			return true;
		}
	}
	*/

	// Set the TCP_NODELAY flag on the socket
	{
		int yes = 1;
		if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int)) < 0)
		{
			// Do nothing but warn
			LOG(LOG_WARN, "Could not set tcp_nodelay on service client tcp socket: %s", strerror(errno));

			close(sock);
			return true;
		}
	}

	// Set non-blocking on socket
	{
		int cntl = fcntl(sock, F_GETFL, 0);
		if (cntl < 0)
		{
			LOG(LOG_WARN, "Could not get status flags on service client tcp socket: %s", strerror(errno));

			close(sock);
			return true;
		}

		if (fcntl(sock, F_SETFL, cntl | O_NONBLOCK) < 0)
		{
			LOG(LOG_WARN, "Could not set nonblock status flag on service client tcp socket: %s", strerror(errno));

			close(sock);
			return true;
		}
	}

	client_t * client = NULL;

	// Get new client object
	{
		exception_t * e = NULL;
		client = client_new(stream, &e);
		if (client == NULL || exception_check(&e))
		{
			LOG(LOG_WARN, "Service tcp client register error: %s", exception_message(e));
			exception_free(e);

			close(sock);
			return true;
		}
	}

	// Init the tcpstream
	tcpclient_t * tcpclient = client_data(client);
	memset(tcpclient, 0, sizeof(tcpclient_t));
	watcher_newfd(&tcpclient->socket, sock, FD_EDGE_TRIG | FD_READ | FD_WRITE, tcp_newdata, client);
	tcpclient->ip = addr.sin_addr.s_addr;
	tcpclient->payload = NULL;

	LOG(LOG_DEBUG, "New tcp service client from %s", addr2string(tcpclient->ip).string);

	// Add it to the mainloop
	{
		exception_t * e = NULL;
		if (!mainloop_addwatcher(stream_mainloop(stream), &tcpclient->socket, &e) || exception_check(&e))
		{
			LOG(LOG_WARN, "Could not add tcp client to mainloop: %s", exception_message(e));
			exception_free(e);

			client_destroy(client);
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

	int fd = tcp_server(tcp_port, err);
	if (fd < 0 || exception_check(err))
	{
		return false;
	}

	stream_t * stream = stream_new("tcp", sizeof(tcpstream_t), tcp_streamdestroy, sizeof(tcpclient_t), tcp_clientsend, tcp_clientheartbeat, tcp_clientcheck, tcp_clientdestroy, err);
	if (stream == NULL || exception_check(err))
	{
		return false;
	}

	tcpstream_t * tcpstream = stream_data(stream);
	watcher_newfd(&tcpstream->socket, fd, FD_READ, tcp_newclient, stream);
	if (!mainloop_addwatcher(stream_mainloop(stream), &tcpstream->socket, err) || exception_check(err))
	{
		stream_destroy(stream);
		return false;
	}

	return true;
}
