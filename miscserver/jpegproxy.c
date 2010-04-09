#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <kernel.h>
#include <buffer.h>
#include <aul/common.h>

DEF_BLOCK(jpegproxy, jpegproxy_new, "si");
BLK_ONUPDATE(jpegproxy, jpegproxy_update);
BLK_INPUT(jpegproxy, frame, "x");


typedef struct
{
	kobject_t kobject;
	int sock;
	const char * host;
	int port;
} proxy_t;

static void doconnect(proxy_t * proxy)
{
	if (proxy->sock != -1)
	{
		return;
	}

	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	String port = string_new("%d", proxy->port);
	if ((rv = getaddrinfo(proxy->host, port.string, &hints, &servinfo)) != 0) {
		LOG(LOG_ERR, "Proxy: Could not call getaddrinfo: %s", gai_strerror(rv));
		return;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			LOG(LOG_WARN, "Proxy: could not connect to proxy: %s", strerror(errno));
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			LOG(LOG_WARN, "Proxy: count not connect to proxy: %s", strerror(errno));
			continue;
		}

		break;
	}

	if (p == NULL) {
		LOG(LOG_ERR, "Proxy: failed to connect to client");
		return;
	}

	freeaddrinfo(servinfo);
	proxy->sock = sockfd;
}

static char * jpegproxy_info(void * object)
{
	char * str = "[SERVER INFO PLACEHOLDER]";
	return strdup(str);
}

static void jpegproxy_destroy(void * object)
{
	//do nothing
}

void * jpegproxy_new(const char * host, int port)
{
	String name = string_new("PROXY (port %d)", port);

	proxy_t * p = kobj_new("JpegProxyStream", string_copy(&name), jpegproxy_info, jpegproxy_destroy, sizeof(proxy_t));
	p->sock = -1;
	p->host = strdup(host);
	p->port = port;

	doconnect(p);
	return p;
}

void jpegproxy_update(void * data)
{
	proxy_t * proxy = data;
	if (proxy->sock == -1)
	{
		doconnect(proxy);
		return;
	}

	LOG(LOG_INFO, "HERE");
	if (ISNULL(frame))
	{
		return;
	}
	LOG(LOG_INFO, "HERE2");

	buffer_t frame = INPUTT(buffer_t, frame);
	ssize_t sent = send(proxy->sock, frame, buffer_size(frame), MSG_DONTWAIT);

	if (sent != buffer_size(frame))
	{
		LOG(LOG_WARN, "Proxy: could not send all data to proxy (sent %d out of %d). DESYNC", sent, buffer_size(frame));
		close(proxy->sock);
		proxy->sock = -1;
	}

	LOG(LOG_INFO, "SENT %d", buffer_size(frame));
}
