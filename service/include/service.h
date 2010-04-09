#ifndef __SERVICE_H
#define __SERVICE_H

#include <stdlib.h>
#include <inttypes.h>


#ifdef __cplusplus
extern "C" {
#endif

#define SERVICE_SENDER_THREADS		1

#define SERVICE_TIMEOUT_US			5000000LL	/* 5 seconds */
#define SERVICE_FRAGSIZE			10000

#define SERVICE_CLIENTS_MAX			4
#define SERVICE_PACKETS_MAX			1000

#define SERVICE_PORT_MIN			1
#define SERVICE_PORT_MAX			65000

#define S_LIST_HANDLE				"__list"
#define S_ALIVE_HANDLE				"__alive"
#define S_ECHO_HANDLE				"__echo"
#define S_LOG_HANDLE				"log"

/* common formats */
#define TXT							"TXT"		/* human readable text */
#define XML							"XML"		/* xml data */
#define CSV							"CSV"		/* comma-seperated values */
#define JPEG						"JPEG"		/* jpeg image (full image per packet) */
#define RAW							"RAW"		/* raw (unspecified) format */


typedef const char * sid_t;
typedef sid_t service_h;
typedef sid_t client_h;
typedef sid_t stream_h;

typedef void (*connect_f)(service_h service, stream_h stream);
typedef void (*disconnect_f)(service_h service, stream_h stream);
typedef void (*clientdata_f)(service_h service, stream_h stream, uint64_t timestamp_us, const void * data, size_t len);

service_h service_register(const char * id, const char * name, const char * format, const char * params, const char * desc, connect_f newconnect, disconnect_f disconnected, clientdata_f clientdata);
void service_writedata(service_h service, uint64_t timestamp_us, const void * data, size_t length);
void service_writeclientdata(service_h service, stream_h stream, uint64_t timestamp_us, const void * data, size_t length);

#ifdef __cplusplus
}
#endif
#endif