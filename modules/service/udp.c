#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <linux/tcp.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <aul/string.h>
#include <aul/stack.h>
#include <aul/net.h>

#include <kernel.h>

#include <service.h>


#define UDP_HEADER_SIZE			17
#define UDP_BODY_SIZE			(512 - UDP_HEADER_SIZE)


int udp_port = DEFAULT_UDP_PORT;
double udp_timeout = DEFAULT_NET_TIMEOUT;

module_config(udp_port, 'i', "UDP port to listen for service requests and send service data on");
module_config(udp_timeout, 'd', "UDP timeout (in seconds) with no heartbeat before client is disconnect");

typedef struct
{
	fdwatcher_t socket;
	stack_t clients;
} udpstream_t;

typedef struct
{
	client_t * client;
	list_t stream_list;

	in_addr_t ip;
	in_port_t port;

	int64_t microtimestamp;
	buffer_t * payload;
	uint32_t packetnum;
	uint32_t numpackets;
} udpclient_t;


static inline string_t addr2string(in_addr_t ip)
{
	unsigned char a4 = (ip & 0xFF000000) >> 24;
	unsigned char a3 = (ip & 0x00FF0000) >> 16;
	unsigned char a2 = (ip & 0x0000FF00) >> 8;
	unsigned char a1 = ip & 0xFF;
	return string_new("%d.%d.%d.%d", a1, a2, a3, a4);
}

static inline bool udp_writepacket(int sock, struct sockaddr_in * addr, int64_t microtimestamp, const buffer_t * data, uint32_t packetnum)
{
	int64_t ts = microtimestamp;
	uint32_t s = buffer_size(data);
	uint32_t pn = packetnum;

	#define a(x)	((uint8_t *)&x)
	uint8_t buffer[512] = {
		SC_DATA,																			// Control byte
		a(ts)[0], a(ts)[1], a(ts)[2], a(ts)[3], a(ts)[4], a(ts)[5], a(ts)[6], a(ts)[7],		// 64 bit microsecond timestamp
		a(s)[0], a(s)[1], a(s)[2], a(s)[3],													// Size of data payload to follow
		a(pn)[0], a(pn)[1], a(pn)[2], a(pn)[3],												// Packet number
		0																					// Rest...
	};
	#undef a

	size_t read = buffer_read(data, &buffer[UDP_HEADER_SIZE], packetnum * UDP_BODY_SIZE, UDP_BODY_SIZE) + UDP_HEADER_SIZE;
	ssize_t wrote = sendto(sock, buffer, read, 0, (struct sockaddr *)addr, sizeof(struct sockaddr_in));

	//LOG(LOG_INFO, "Write packet %u, R: %zu, W: %zd -> %s:%u %u", packetnum, read, wrote, addr2string(addr->sin_addr.s_addr).string, ntohs(addr->sin_port), addr->sin_family);

	return read == wrote;
}

static void udp_streamdestroy(stream_t * stream)
{
	udpstream_t * udpstream = stream_data(stream);

	// Destroy the udp watcher
	{
		exception_t * e = NULL;
		if (!mainloop_removewatcher(&udpstream->socket, &e) || exception_check(&e))
		{
			LOG(LOG_WARN, "Could not remove udp stream watcher: %s", exception_message(e));
			exception_free(e);
		}
	}
}

static bool udp_clientsend(client_t * client, int64_t microtimestamp, const buffer_t * data)
{
	udpclient_t * udpclient = client_data(client);
	udpstream_t * udpstream = stream_data(client_stream(client));

	// Check to make sure there isn't a deferred buffer still
	{
		if (udpclient->payload != NULL)
		{
			LOG(LOG_WARN, "Could send complete service data payload to udp client before new data payload. Dropping client");
			return false;
		}
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = udpclient->port;
	addr.sin_addr.s_addr = udpclient->ip;

	size_t packetnum = 0;
	size_t numpackets = (buffer_size(data) + UDP_BODY_SIZE - 1) / UDP_BODY_SIZE;

	// Try to dump all the packets to the client
	for (; packetnum < numpackets; packetnum++)
	{
		if (!udp_writepacket(watcher_fd(&udpstream->socket), &addr, microtimestamp, data, packetnum))
		{
			break;
		}
	}

	if (packetnum != numpackets)
	{
		LOG(LOG_INFO, "DEFER PACKET");

		// Could not send out all packets, save buffer and defer
		udpclient->microtimestamp = microtimestamp;
		udpclient->payload = buffer_dup(data);
		udpclient->packetnum = packetnum;
		udpclient->numpackets = numpackets;

		exception_t * e = NULL;
		if (!mainloop_rearmwatcher(&udpstream->socket, &e) || exception_check(&e))
		{
			LOG(LOG_WARN, "Client udp socket buffer defer error: %s", exception_message(e));
			exception_free(e);
		}
	}

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
	sendto(watcher_fd(&udpstream->socket), &data, sizeof(uint8_t), 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
}

static bool udp_clientcheck(client_t * client)
{
	// Return false if client hasn't heartbeat'd since more than DEFAULT_NET_TIMEOUT ago
	return (client_lastheartbeat(client) + DEFAULT_NET_TIMEOUT) > kernel_timestamp();
}

static void udp_clientdestroy(client_t * client)
{
	udpclient_t * udpclient = client_data(client);

	// Free the payload if set
	{
		if (udpclient->payload != NULL)
		{
			buffer_free(udpclient->payload);
			udpclient->payload = NULL;
		}
	}

	list_remove(&udpclient->stream_list);
}

static bool udp_newdata(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{
	stream_t * stream = userdata;
	udpstream_t * udpstream = stream_data(stream);

	if (cond & FD_READ)
	{
		// There is pending data to be read on the socket

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

			LOG(LOG_DEBUG, "New udp service client from %s:%u", addr2string(ip).string, ntohs(port));

			// Initialize the udpclient
			udpclient = client_data(client);
			udpclient->client = client;
			udpclient->ip = ip;
			udpclient->port = port;

			// Add the udpclient to the stream list
			stack_enqueue(&udpstream->clients, &udpclient->stream_list);
		}


		ssize_t size = client_control(client, buffer, read);
		if (size <= 0)
		{
			// Close client or did not receive complete packet
			client_destroy(client);
		}
	}

	if (cond & FD_WRITE && !stack_isempty(&udpstream->clients))
	{
		// TODO - figure out a better way to loop than this INFINATE LOOP!
		while (true)
		{
			udpclient_t * udpclient = NULL;

			// TODO IMPORTANT - figure out a way to prioritize this so the early on clients don't get all the attention!!!

			list_t * pos = NULL, * n = NULL;
			list_foreach_safe(pos, n, &udpstream->clients)
			{
				udpclient_t * testudpclient = list_entry(pos, udpclient_t, stream_list);

				// Move entry to the end of the stack
				//list_remove(pos);
				//stack_enqueue(&udpstream->clients, pos);

				if (testudpclient->payload != NULL)
				{
					udpclient = testudpclient;
				}
			}

			if (udpclient == NULL)
			{
				break;
			}

			// There is pending data to be sent to a udp client
			struct sockaddr_in addr;
			memset(&addr, 0, sizeof(struct sockaddr_in));
			addr.sin_family = AF_INET;
			addr.sin_port = udpclient->port;
			addr.sin_addr.s_addr = udpclient->ip;

			if (!udp_writepacket(fd, &addr, udpclient->microtimestamp, udpclient->payload, udpclient->packetnum))
			{
				break;
			}

			udpclient->packetnum += 1;
			if (udpclient->packetnum == udpclient->numpackets)
			{
				buffer_free(udpclient->payload);
				udpclient->payload = NULL;
			}
		}
	}

	return true;
}

bool udp_init(exception_t ** err)
{
	if (udp_port <= 0)
	{
		LOG(LOG_INFO, "Service udp stream disabled (invalid udp port configuration)");
		return true;
	}

	int sock = udp_server(udp_port, err);
	if (sock < 0 || exception_check(err))
	{
		return false;
	}

	/*
	// Set socket type-of-service
	{
		int tos = IPTOS_LOWDELAY;
		if (setsockopt(sock, IPPROTO_IP, IP_TOS, &tos, sizeof(int)) < 0)
		{
			// Do nothing but warn
			LOG(LOG_WARN, "Could not set type-of-service of service client tcp socket: %s", strerror(errno));

			close(sock);
			return true;
		}
	}
	*/

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

	stream_t * stream = stream_new("udp", sizeof(udpstream_t), udp_streamdestroy, sizeof(udpclient_t), udp_clientsend, udp_clientheartbeat, udp_clientcheck, udp_clientdestroy, err);
	if (stream == NULL || exception_check(err))
	{
		return false;
	}

	udpstream_t * udpstream = stream_data(stream);
	watcher_newfd(&udpstream->socket, sock, FD_EDGE_TRIG | FD_READ | FD_WRITE, udp_newdata, stream);
	stack_init(&udpstream->clients);

	if (!mainloop_addwatcher(stream_mainloop(stream), &udpstream->socket, err) || exception_check(err))
	{
		stream_destroy(stream);
		return false;
	}

	return true;
}
