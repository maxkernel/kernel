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


int udp_server(uint16_t port, exception_t ** err)
{
	if (exception_check(err))
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Error already set in function udp_server");
		return -1;
	}

	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1)
	{
		exception_set(err, errno, "Could not create datagram socket: %s", strerror(errno));
		return -1;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		exception_set(err, errno, "Could not bind datagram to port %d: %s", port, strerror(errno));
		close(sock);
		return -1;
	}

	return sock;
}

int tcp_server(uint16_t port, exception_t ** err)
{
	if (exception_check(err))
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Error already set in function tcp_server");
		return -1;
	}

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1)
	{
		exception_set(err, errno, "Could not create tcp socket: %s", strerror(errno));
		return -1;
	}

	int yes = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
	{
		exception_set(err, errno, "Could not set socket options: %s", strerror(errno));
		close(sock);
		return -1;
	}

	// Set O_NONBLOCK on the file descriptor
	int flags = fcntl(sock, F_GETFL, 0);
	if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Could not set non-blocking option on tcp socket to port %d: %s", port, strerror(errno));
	}


	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		exception_set(err, errno, "Could not bind socket to port %d: %s", port, strerror(errno));
		close(sock);
		return -1;
	}

	if (listen(sock, AUL_NET_BACKLOG) == -1)
	{
		exception_set(err, errno, "Could not listen for incoming connections on port %d: %s", port, strerror(errno));
		close(sock);
		return -1;
	}

	return sock;
}


int unix_server(const char * path, exception_t ** err)
{
	if (exception_check(err))
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Error already set in function unix_server");
		return -1;
	}

	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1)
	{
		exception_set(err, errno, "Could not create unix socket %s: %s", path, strerror(errno));
		return -1;
	}

	if (unlink(path) == -1)
	{
		if (errno != ENOENT)
		{
			exception_set(err, errno, "Could not create unix socket file %s: %s", path, strerror(errno));
			close(sock);
			return -1;
		}
	}

	// Set O_NONBLOCK on the file descriptor
	int flags = fcntl(sock, F_GETFL, 0);
	if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Could not set non-blocking option on unix socket %s: %s", path, strerror(errno));
	}


	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr.sun_family) + strlen(addr.sun_path)) == -1)
	{
		exception_set(err, errno, "Could not bind unix socket %s: %s", path, strerror(errno));
		close(sock);
		return -1;
	}

	if (listen(sock, AUL_NET_BACKLOG) == -1)
	{
		exception_set(err, errno, "Could not listen for incoming connections on socket %s: %s", path, strerror(errno));
		close(sock);
		return -1;
	}

	return sock;
}

int unix_client(const char * path, exception_t ** err)
{
	if (exception_check(err))
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Error already set in function unix_client");
		return -1;
	}

	int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock == -1)
	{
		exception_set(err, errno, "Could not create unix socket %s: %s", path, strerror(errno));
		return -1;
	}

	// Set O_NONBLOCK on the file descriptor
	int flags = fcntl(sock, F_GETFL, 0);
	if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Could not set non-blocking option on unix socket %s: %s", path, strerror(errno));
	}


	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr.sun_family) + strlen(addr.sun_path)) == -1)
	{
		exception_set(err, errno, "Could not connect unix socket %s: %s", path, strerror(errno));
		close(sock);
		return -1;
	}

	return sock;
}
