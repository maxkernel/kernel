#ifndef __VIDEOSERVER_H
#define __VIDEOSERVER_H

typedef struct
{
	const char * class_name;
	char * obj_name;
	unsigned int obj_id;
	info_f info;
	destructor_f destructor;

	unsigned int port;
	void * socket;
	GSList * clients;
	GMutex * mutex;
} server_t;

typedef struct
{
	server_t * parent;
	void * socket;
	GIOChannel * gio;
} client_t;


#define GETIP(sock)			(gnet_inetaddr_get_canonical_name(gnet_tcp_socket_get_remote_inetaddr(sock)))


#endif
