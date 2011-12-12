#include <unistd.h>
#include <errno.h>

#include <aul/mainloop.h>
#include <aul/net.h>

#include <kernel.h>
#include <service.h>
#include "internal.h"

CFG_PARAM(tcp_port, S_INTEGER, "TCP port to listen for service requests and send service data on");


extern mainloop_t * serviceloop;

int tcp_port = DEFAULT_TCP_PORT;
static tcpstream_t tcpstreams[SERVICE_CLIENTS_MAX];




static void tcp_free(stream_t * data)
{
	tcpstream_t * tcp = (tcpstream_t *)data;

	mainloop_removewatch(serviceloop, tcp->sockfd, FD_READ);
	close(tcp->sockfd);
}


static void tcp_send(stream_t * data, packet_t * packet)
{
	tcpstream_t * tcp = (tcpstream_t *)data;
	send(tcp->sockfd, packet->data.raw, packet->size, 0);
	//LOG(LOG_INFO, "TS: %lld", packet->data.header.timestamp);
}


static bool tcp_newdata(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{
	tcpstream_t * data = userdata;

	ssize_t bytesread = recv(fd, data->packet.data.raw + data->packet.size, PACKET_SIZE - data->packet.size, 0);
	if (bytesread <= 0)
	{
		string_t errstr = string_blank();
		if (bytesread < 0)
		{
			string_append(&errstr, " (%s)", strerror(errno));
		}
		LOG(LOG_DEBUG, "Service client (%s) disconnected TCP%s", data->stream.client == NULL ? "?" : data->stream.client->handle, errstr.string);

		service_freestream((stream_t *)data);
		return false;
	}
	data->packet.size += bytesread;

	//try to resync if packet has desync'd
	if (data->state == DESYNC)
	{
		if (data->packet.size >= 8)
		{
			size_t i=0;
			for (; i<(data->packet.size - 8); i++)
			{
				if (*(uint64_t *)(&data->packet.data.raw[i]) == PACKET_SYNC)
				{
					//we have resync'd
					memmove(data->packet.data.raw, &data->packet.data.raw[i], data->packet.size - i);
					data->packet.size -= i;
					data->state = DESYNC;
					break;
				}
			}
		}

		if (data->state == DESYNC)
		{
			//we are still desync'd
			data->packet.size = 0;
			return true;
		}
	}

	//parse packet header
	while (data->packet.size >= HEADER_LENGTH)
	{
		if (data->packet.data.header.sync != PACKET_SYNC)
		{
			LOG(LOG_WARN, "Service client '%s' has desynchronized (TCP)", data->stream.client == NULL ? "?" : data->stream.client->handle);

			data->state = DESYNC;
			return true;
		}

		if (data->packet.data.header.frame_length > SERVICE_FRAGSIZE)
		{
			LOG(LOG_WARN, "Service client '%s' packet size too big (TCP)", data->stream.client == NULL ? "?" : data->stream.client->handle);

			data->packet.size = 0;
			return true;
		}

		if (data->packet.size >= (data->packet.data.header.frame_length + HEADER_LENGTH))
		{
			//we have the full packet
			packet_prepare(&data->packet);

			if (data->stream.client == NULL)
			{
				//we don't have client struct, get it
				client_t * client = service_getclient(data->packet.data.header.client_handle);
				if (client == NULL)
				{
					//out of memory
					service_freestream(&data->stream);
					return false;
				}

				bool iserror = false;
				mutex_lock(&client->lock);
				{
					if (client->streams[TCP] != NULL)
					{
						LOG(LOG_WARN, "Service client '%s' already has a TCP client", client->handle);
						iserror = true;
					}

					client->streams[TCP] = (stream_t *)data;
				}
				mutex_unlock(&client->lock);

				if (iserror)
				{
					service_freestream(&data->stream);
					return false;
				}

				data->stream.client = client;
				LOG(LOG_DEBUG, "TCP streaming has been added to service client (%s)", client->handle);

				//start streaming default services
				service_startstream_default(data->stream.handle);
			}

			data->stream.client->timeout_us = kernel_timestamp();
			service_dispatchdata(data->packet.data.header.service_handle, data->stream.client->handle, data->stream.handle, data->packet.data.header.timestamp, data->packet.data.packet.payload, data->packet.data.header.frame_length);

			size_t packetsize = data->packet.data.header.frame_length + HEADER_LENGTH;
			memmove(data->packet.data.raw, data->packet.data.raw + packetsize, data->packet.size - packetsize);
			data->packet.size -=  packetsize;
		}
	}

	return true;
}


static bool tcp_newclient(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{
	struct sockaddr_in addr;
	socklen_t socklen = sizeof(addr);
	int sock = accept(fd, (struct sockaddr *)&addr, &socklen);
	if (sock == -1)
	{
		LOG(LOG_WARN, "Could not accept new service client socket: %s", strerror(errno));
	}
	else
	{
		tcpstream_t * data = service_newstream(tcpstreams, TCP, tcp_send, tcp_free, sizeof(tcpstream_t));
		if (data != NULL)
		{
			data->sockfd = sock;
			data->state = OKAY;

			string_t str_addr = addr2string(addr.sin_addr.s_addr);
			mainloop_addwatch(serviceloop, sock, FD_READ, tcp_newdata, data);
			LOG(LOG_DEBUG, "New TCP service client (%s)", str_addr.string);
		}
	}

	return true;
}

string_t tcp_streamconfig()
{
	return string_new("service_tcpport=%d\n", tcp_port);
}

void tcp_init()
{
	if (tcp_port != 0)
	{
		if (CLAMP(tcp_port, SERVICE_PORT_MIN, SERVICE_PORT_MAX) == tcp_port)
		{
			exception_t * err = NULL;
			int tcp_fd = tcp_server(tcp_port, &err);

			if (err != NULL)
			{
				LOG(LOG_ERR, "Could not create service TCP server on port %d: %s", tcp_port, err->message);
				exception_free(err);
			}
			else
			{
				mainloop_addwatch(serviceloop, tcp_fd, FD_READ, tcp_newclient, NULL);
				LOG(LOG_DEBUG, "Service TCP server awaiting clients on port %d", tcp_port);
			}
		}
		else
		{
			LOG(LOG_ERR, "Invalid service TCP port specified. %d (min) <= %d <= %d (max)", SERVICE_PORT_MIN, tcp_port, SERVICE_PORT_MAX);
		}
	}
}

