#ifndef __INTERNAL_H
#define __INTERNAL_H

#include <stdlib.h>
#include <inttypes.h>
#include <glib.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <aul/string.h>
#include <aul/mutex.h>
#include <aul/mainloop.h>

#include <kernel.h>
#include <service.h>

#ifdef __cplusplus
extern "C" {
#endif


#define HANDLE_MAXLEN			8
#define PROTOCOL_NUM			2
#define STREAMS_PER_SERVICE		( SERVICE_CLIENTS_MAX * PROTOCOL_NUM )

#define HEADER_LENGTH			40		/* length of the header */
#define PACKET_SIZE				(HEADER_LENGTH + SERVICE_FRAGSIZE)
#define PACKET_SYNC				0xFFFFFFFFFFFFFFFFLL


#define DEFAULT_TCP_PORT	10001
#define DEFAULT_UDP_PORT	10002


typedef struct
{
	uint32_t size;
	union
	{
		struct
		{
			uint64_t sync;
			char service_handle[HANDLE_MAXLEN];
			char client_handle[HANDLE_MAXLEN];
			uint32_t frame_length;
			uint16_t frag_size;
			uint16_t frag_num;
			uint64_t timestamp;
		} header;
		struct
		{
			unsigned char header[HEADER_LENGTH];
			unsigned char payload[SERVICE_FRAGSIZE];
		} packet;
		unsigned char raw[PACKET_SIZE];
	} data;
} packet_t;

typedef enum
{
	TCP		= 0,
	UDP		= 1
} protocol_t;

struct __stream_t;
struct __service_t;
struct __client_t;

typedef void (*psend_f)(struct __stream_t * data, packet_t * packet);
typedef void (*pdestroy_f)(struct __stream_t * data);

typedef struct __stream_t
{
	bool inuse;

	char handle[HANDLE_MAXLEN];
	protocol_t protocol;
	struct __client_t * client;

	psend_f send;
	pdestroy_f destroy;
} stream_t;

typedef struct
{
	stream_t stream;

	int sockfd;
	enum
	{
		OKAY,
		DESYNC
	} state;

	packet_t packet;
} tcpstream_t;

typedef struct
{
	stream_t stream;

	int sockfd;
	struct sockaddr_in addr;
} udpstream_t;



typedef struct __service_t
{
	kobject_t object;
	char handle[HANDLE_MAXLEN];

	const char * name;
	const char * format;
	const char ** params;
	const char * desc;

	mutex_t lock;

	connect_f connect;
	disconnect_f disconnect;
	clientdata_f clientdata;

	stream_t * streams[STREAMS_PER_SERVICE];
} service_t;

typedef struct __client_t
{
	bool inuse;
	char handle[HANDLE_MAXLEN];

	int64_t timeout_us;
	mutex_t lock;

	stream_t * streams[PROTOCOL_NUM];
} client_t;

extern mutex_t servicelock;

void send_init();
void send_data(service_h service_handle, client_h client_handle, stream_t * stream, uint64_t timestamp, const void * data, size_t length);
void send_startthread(void * userdata);
void send_stopthread(void * userdata);

void tcp_init();
void udp_init();

String tcp_streamconfig();
String udp_streamconfig();

void * service_newstream(void * array, protocol_t protocol, psend_f send, pdestroy_f destroy, size_t size);
void service_freestream(stream_t * stream);

client_t * service_getclient(char * client_handle);

void service_default_init();
void service_dispatchdata(service_h service_handle, client_h client_handle, stream_h stream_handle, uint64_t timestamp_us, void * data, size_t len);
void service_startstream_default(stream_h stream_handle);
void service_startstream(service_h service_handle, stream_h stream_handle);
void service_stopstream(service_h service_handle, stream_h stream_handle);


static inline bool isunique(char * str, GHashTable * table)
{
	bool unique;

	mutex_lock(&servicelock);
	{
		unique = g_hash_table_lookup(table, str) == NULL;
	}
	mutex_unlock(&servicelock);

	return unique;
}

static inline void generateid(char * target, GHashTable * table)
{
	srand(time(NULL));
	do {
		target[0] = '_';
		target[1] = rand()%26+'a';
		target[2] = rand()%26+'a';
		target[3] = rand()%26+'A';
		target[4] = rand()%26+'A';
		target[5] = rand()%26+'A';
		target[6] = rand()%10+'0';
		target[7] = 0;
	} while (!isunique(target, table));
}

static inline String addr2string(uint32_t ip)
{
	unsigned char a4 = (ip & 0xFF000000) >> 24;
	unsigned char a3 = (ip & 0x00FF0000) >> 16;
	unsigned char a2 = (ip & 0x0000FF00) >> 8;
	unsigned char a1 = ip & 0xFF;
	return string_new("%d.%d.%d.%d", a1, a2, a3, a4);
}

static inline void packet_prepare(packet_t * packet)
{
	//force packet header to have zeros in the right places
	packet->data.header.service_handle[HANDLE_MAXLEN-1] = 0;
	packet->data.header.client_handle[HANDLE_MAXLEN-1] = 0;
}


#ifdef __cplusplus
}
#endif
#endif
