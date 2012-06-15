#ifndef __SERVICE_H
#define __SERVICE_H

#include <stdlib.h>
#include <inttypes.h>

#include <aul/common.h>
#include <aul/mainloop.h>

#include <buffer.h>
#include <kernel.h>


#ifdef __cplusplus
extern "C" {
#endif

//#define SERVICE_SENDER_THREADS		1

#define SERVICE_TIMEOUT_NS			(5 * NANOS_PER_SECOND)		// 5 seconds
/*
#define SERVICE_FRAGSIZE			10000

#define SERVICE_CLIENTS_MAX			4
#define SERVICE_PACKETS_MAX			1000

#define SERVICE_PORT_MIN			1
#define SERVICE_PORT_MAX			65000
*/

// Common formats
#define TXT							"TXT"		// Human readable text
//#define XML							"XML"		// XML formatted data
#define CSV							"CSV"		// comma-seperated values
#define JPEG						"JPEG"		// jpeg image (full image per packet)
//#define RAW							"RAW"		// raw (unspecified) format

/*
typedef const char * sid_t;
typedef sid_t service_h;
typedef sid_t client_h;
typedef sid_t stream_h;

typedef void (*connect_f)(service_h service, stream_h stream);
typedef void (*disconnect_f)(service_h service, stream_h stream);
typedef void (*clientdata_f)(service_h service, stream_h stream, uint64_t timestamp_us, const void * data, size_t len);

service_h service_register(const char * id, const char * name, const char * format, const char * params, const char * desc, connect_f newconnect, disconnect_f disconnected, clientdata_f clientdata);
void service_writedata(service_h service, uint64_t timestamp_us, const buffer_t buffer);
void service_writeclientdata(service_h service, stream_h stream, uint64_t timestamp_us, const buffer_t buffer);
*/

#define SERVICE_CLIENTS_PER_STREAM		25

#define DEFAULT_TCP_PORT	10001
#define DEFAULT_UDP_PORT	10002

typedef struct __service_t service_t;
typedef struct __stream_t stream_t;
typedef struct __client_t client_t;

typedef void (*streamsend_f)(client_t * client, buffer_t data);
typedef bool (*streamcheck_f)(client_t * client);
typedef void (*clientdestroy_f)(client_t * client);


struct __service_t
{
	kobject_t kobject;
	list_t service_list;

	mutex_t service_lock;

	char * name;
	char * desc;

	list_t clients;
};

struct __stream_t
{
	kobject_t kobject;

	mainloop_t * loop;
	mutex_t stream_lock;

	streamsend_f sender;
	streamcheck_f checker;

	list_t clients;
};

struct __client_t
{
	list_t service_list;
	list_t stream_list;

	service_t * service;
	stream_t * stream;

	clientdestroy_f destroyer;
	uint8_t data[0];
};


stream_t * service_newstream(const char * name, size_t objectsize, streamsend_f sender, streamcheck_f checker, clientdestroy_f destroyer, exception_t ** err);

client_t * stream_newclient(stream_t * stream, exception_t ** err);
#define stream_mainloop(s)	((s)->loop)


#define client_data(c)		((void *)(c)->data)


#ifdef __cplusplus
}
#endif
#endif
