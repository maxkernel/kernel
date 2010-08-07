#include <errno.h>

#include <aul/mainloop.h>
#include <aul/net.h>

#include <kernel.h>
#include <service.h>
#include "internal.h"

CFG_PARAM(udp_port, S_INTEGER, "UDP port to listen for service requests and send service data on");


extern mainloop_t * serviceloop;

int udp_port = DEFAULT_UDP_PORT;
static udpstream_t udpstreams[SERVICE_CLIENTS_MAX];

static void udp_free(stream_t * data)
{
	//do nothing
}

static void udp_send(stream_t * data, packet_t * packet)
{
	udpstream_t * stream = (udpstream_t *)data;
	ssize_t s = sendto(stream->sockfd, packet->data.raw, packet->size, 0, &stream->addr, sizeof(struct sockaddr_in));

	if (s <= 0)
	{
		LOG(LOG_WARN, "Error while sending UDP service packet to %s (error: %zd, reason: %s)", stream->stream.client->handle, s, strerror(errno));
	}
}

static bool udp_newdata(int fd, fdcond_t cond, void * userdata)
{
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	packet_t packet;

	ssize_t bytesread = recvfrom(fd, packet.data.raw, PACKET_SIZE, 0, (struct sockaddr *)&addr, &addrlen);
	if (bytesread >= HEADER_LENGTH)
	{
		packet.size = bytesread;
		packet_prepare(&packet);

		if (packet.data.header.sync != PACKET_SYNC)
		{
			LOG(LOG_WARN, "Service client (?) has desynchronized (UDP)");
			return true;
		}

		if (packet.data.header.frame_length > SERVICE_FRAGSIZE)
		{
			//LOG(LOG_INFO, "SIZE=%d", packet.data.header.frame_length);
			LOG(LOG_WARN, "Service client (?) packet size too big (UDP)");
			return true;
		}

		if (packet.size != (packet.data.header.frame_length + HEADER_LENGTH))
		{
			LOG(LOG_WARN, "Service client (?) invalid packet length (UDP)");
			return true;
		}

		client_t * client = service_getclient(packet.data.header.client_handle);
		if (client == NULL)
		{
			//out of memory
			return true;
		}

		bool iserror = false;
		udpstream_t * stream = NULL;
		mutex_lock(&client->lock);
		{
			if (client->streams[UDP] == NULL)
			{
				stream = service_newstream(udpstreams, UDP, udp_send, udp_free, sizeof(udpstream_t));
				if (stream == NULL)
				{
					//out of memory
					iserror = true;
				}
				else
				{
					stream->sockfd = fd;
					memcpy(&stream->addr, &addr, sizeof(struct sockaddr_in));
					client->streams[UDP] = (stream_t *)stream;

					stream->stream.client = client;
					LOG(LOG_DEBUG, "UDP streaming has been added to service client (%s)", client->handle);

					//start streaming default services
					service_startstream_default(stream->stream.handle);
				}
			}
			else
			{
				stream = (udpstream_t *)client->streams[UDP];

				if (stream->addr.sin_addr.s_addr != addr.sin_addr.s_addr || stream->addr.sin_port != addr.sin_port)
				{
					String real = addr2string(stream->addr.sin_addr.s_addr);
					String spoof = addr2string(addr.sin_addr.s_addr);
					LOG(LOG_WARN, "Service client (%s) with registered address (%s:%d) is trying to be spoofed by address (%s:%d)", client->handle, real.string, stream->addr.sin_port, spoof.string, addr.sin_port);
					iserror = true;
				}
			}
		}
		mutex_unlock(&client->lock);

		if (!iserror)
		{
			client->timeout_us = kernel_timestamp();
			service_dispatchdata(packet.data.header.service_handle, client->handle, stream->stream.handle, packet.data.header.timestamp, packet.data.packet.payload, packet.data.header.frame_length);
		}
	}

	return true;
}

String udp_streamconfig()
{
	return string_new("service_udpport=%d\n", udp_port);
}

void udp_init()
{
	if (udp_port != 0)
	{
		if (CLAMP(udp_port, SERVICE_PORT_MIN, SERVICE_PORT_MAX) == udp_port)
		{
			Error * err = NULL;
			int udp_fd = udp_server(udp_port, &err);

			if (err != NULL)
			{
				LOG(LOG_ERR, "Could not create service UDP server on port %d: %s", udp_port, err->message);
				error_free(err);
			}
			else
			{
				mainloop_addwatch(serviceloop, udp_fd, FD_READ, udp_newdata, NULL);
				LOG(LOG_DEBUG, "Service UDP server awaiting clients on port %d", udp_port);
			}
		}
		else
		{
			LOG(LOG_ERR, "Invalid service UDP port specified. %d (min) <= %d <= %d (max)", SERVICE_PORT_MIN, udp_port, SERVICE_PORT_MAX);
		}
	}
}
