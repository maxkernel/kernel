#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <aul/common.h>
#include <aul/log.h>
#include <aul/net.h>


int udp_server(uint16_t port, Error ** err)
{
	if (error_check(err))
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Error already set in function udp_server");
		return -1;
	}

	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1)
	{
		if (err != NULL)
		{
			*err = error_new(errno, "Could not create datagram socket: %s", strerror(errno));
		}
		return -1;
	}

	struct sockaddr_in addr;
	ZERO(addr);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		if (err != NULL)
		{
			*err = error_new(errno, "Could not bind datagram to port %d: %s", port, strerror(errno));
		}
		close(sock);
		return -1;
	}

	return sock;
}

int tcp_server(uint16_t port, Error ** err)
{
	if (error_check(err))
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Error already set in function tcp_server");
		return -1;
	}

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
	{
		if (err != NULL)
		{
			*err = error_new(errno, "Could not create tcp socket: %s", strerror(errno));
		}
		return -1;
	}

	int yes = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
	{
		if (err != NULL)
		{
			*err = error_new(errno, "Could not set socket options: %s", strerror(errno));
		}
		close(sock);
		return -1;
	}

	int flags = fcntl(sock, F_GETFL, 0);
	if (flags != -1)
	{
		//try to set O_NONBLOCK, ignore if we can't
		fcntl(sock, F_SETFL, flags | O_NONBLOCK);
	}


	struct sockaddr_in addr;
	ZERO(addr);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		if (err != NULL)
		{
			*err = error_new(errno, "Could not bind socket to port %d: %s", port, strerror(errno));
		}
		close(sock);
		return -1;
	}

	if (listen(sock, AUL_NET_BACKLOG) == -1)
	{
		if (err != NULL)
		{
			*err = error_new(errno, "Could not listen for incoming connections on port %d: %s", port, strerror(errno));
		}
		close(sock);
		return -1;
	}

	return sock;
}


int unix_server(const char * path, Error ** err)
{
	if (error_check(err))
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Error already set in function unix_server");
		return -1;
	}

	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1)
	{
		if (err != NULL)
		{
			*err = error_new(errno, "Could not create unix socket %s: %s", path, strerror(errno));
		}
		return -1;
	}

	if (unlink(path) == -1)
	{
		if (errno != ENOENT)
		{
			if (err != NULL)
			{
				*err = error_new(errno, "Could not create unix socket file %s: %s", path, strerror(errno));
			}
			close(sock);
			return -1;
		}
	}

	int flags = fcntl(sock, F_GETFL, 0);
	if (flags != -1)
	{
		//try to set O_NONBLOCK, ignore if we can't
		fcntl(sock, F_SETFL, flags | O_NONBLOCK);
	}


	struct sockaddr_un addr;
	ZERO(addr);

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, path);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr.sun_family) + strlen(addr.sun_path)) == -1)
	{
		if (err != NULL)
		{
			*err = error_new(errno, "Could not bind unix socket %s: %s", path, strerror(errno));
		}
		close(sock);
		return -1;
	}

	if (listen(sock, AUL_NET_BACKLOG) == -1)
	{
		if (err != NULL)
		{
			*err = error_new(errno, "Could not listen for incoming connections on socket %s: %s", path, strerror(errno));
		}
		close(sock);
		return -1;
	}

	return sock;
}
