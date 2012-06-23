#ifndef __SERVICE_H
#define __SERVICE_H

#include <stdlib.h>
#include <inttypes.h>

#include <aul/common.h>
#include <aul/list.h>
#include <aul/mainloop.h>

#include <buffer.h>
#include <kernel.h>


#ifdef __cplusplus
extern "C" {
#endif


#define SERVICE_MONITOR_TIMEOUT			(1 * NANOS_PER_SECOND)		// 1 second

/*
// Common formats
#define TXT							"TXT"		// Human readable text
//#define XML							"XML"		// XML formatted data
#define CSV							"CSV"		// comma-seperated values
#define JPEG						"JPEG"		// jpeg image (full image per packet)
//#define RAW							"RAW"		// raw (unspecified) format
*/


#define SC_BUFFERSIZE		128

#define SC_GOODBYE			0x00
#define SC_AUTH				0x01			// TODO - add (optional) authentication
#define SC_HEARTBEAT		0x02
#define SC_SUBSCRIBE		0x03
#define SC_UNSUBSCRIBE		0x04
#define SC_BEGIN			0x05
#define SC_DATA				0x06

#define SC_LISTXML			0x11


#define DEFAULT_NET_TIMEOUT		(3 * MICROS_PER_SECOND)

#define DEFAULT_TCP_PORT		10001
#define DEFAULT_UDP_PORT		10002

typedef struct __service_t service_t;
typedef struct __stream_t stream_t;
typedef struct __client_t client_t;

typedef void (*streamdestroy_f)(stream_t * stream);
typedef bool (*clientsend_f)(client_t * client, int64_t microtimestamp, const buffer_t * data);
typedef void (*clientheartbeat_f)(client_t * client);
typedef bool (*clientcheck_f)(client_t * client);
typedef void (*clientdestroy_f)(client_t * client);


struct __service_t
{
	kobject_t kobject;
	list_t service_list;		// TODO - rename services_list (note the extra 's')

	mutex_t lock;

	char * name;
	char * format;
	char * desc;

	list_t clients;
};

struct __stream_t
{
	kobject_t kobject;

	mainloop_t * loop;
	mutex_t lock;

	streamdestroy_f destroyer;

	list_t clients;
	uint8_t data[0];
};

struct __client_t
{
	list_t service_list;
	list_t stream_list;

	service_t * service;
	mutex_t * lock;

	clientsend_f sender;
	clientheartbeat_f heartbeater;
	clientcheck_f checker;
	clientdestroy_f destroyer;

	bool inuse;
	bool locked;
	int64_t lastheartbeat;
	uint8_t data[0];
};

service_t * service_new(const char * name, const char * format, const char * desc, exception_t ** err);
void service_destroy(service_t * service);
service_t * service_lookup(const char * name);
void service_send(service_t * service, int64_t microtimestamp, const buffer_t * buffer);
bool service_subscribe(service_t * service, client_t * client, exception_t ** err);
void service_unsubscribe(client_t * client);
void service_listxml(buffer_t * buffer);
#define service_name(s)			((s)->name)
#define service_format(s)		((s)->format)
#define service_desc(s)			((s)->desc)
#define service_hasclients(s)	(!list_isempty(&(s)->clients))
#define service_numclients(s)	(list_size(&(s)->clients))

stream_t * stream_new(const char * name, size_t streamsize, streamdestroy_f sdestroyer, size_t clientsize, clientsend_f csender, clientheartbeat_f cheartbeater, clientcheck_f cchecker, clientdestroy_f cdestroyer, exception_t ** err);
void stream_destroy(stream_t * stream);
#define stream_mainloop(s)		((s)->loop)
#define stream_data(s)			((void *)(s)->data)

client_t * client_new(stream_t * stream, exception_t ** err);
void client_destroy(client_t * client);
static inline bool client_send(client_t * client, int64_t microtimestamp, const buffer_t * buffer) { return client->sender(client, microtimestamp, buffer); }
#define client_service(c)		((c)->service)
#define client_lock(c)			((c)->lock)
#define client_inuse(c)			((c)->inuse)
#define client_locked(c)		((c)->locked)
#define client_lastheartbeat(c)	((c)->lastheartbeat)
#define client_data(c)			((void *)(c)->data)

ssize_t clienthelper_control(client_t * client, void * buffer, size_t length);

#ifdef __cplusplus
}
#endif
#endif
