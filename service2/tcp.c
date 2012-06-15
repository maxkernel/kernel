#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <aul/net.h>

#include <kernel.h>

#include <service.h>


int tcp_port = DEFAULT_TCP_PORT;
module_config(tcp_port, 'i', "TCP port to listen for service requests and send service data on");


typedef struct
{
	int fd;
} tcpstream_t;


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

bool tcp_newclient(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
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
		/*
		tcpstream_t * data = service_newstream(tcpstreams, TCP, tcp_send, tcp_free, sizeof(tcpstream_t));
		if (data != NULL)
		{
			data->sockfd = sock;
			data->state = OKAY;

			string_t str_addr = addr2string(addr.sin_addr.s_addr);
			mainloop_addwatchfd(serviceloop, sock, FD_READ, tcp_newdata, data, NULL);
			LOG(LOG_DEBUG, "New TCP service client (%s)", str_addr.string);
		}
		*/

	return true;
}

bool tcp_init(exception_t ** err)
{
	if (tcp_port <= 0)
	{
		LOG(LOG_INFO, "Service tcp stream disabled (invalid tcp port configuration)");
		return true;
	}

	stream_t * stream = service_newstream("tcp", sizeof(tcpstream_t), tcp_send, tcp_check, tcp_destroy, err);
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
