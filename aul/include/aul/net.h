#ifndef __AUL_NET_H
#define __AUL_NET_H

#include <inttypes.h>
#include <aul/error.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUL_NET_BACKLOG		2

int udp_server(uint16_t port, Error ** err);

int tcp_server(uint16_t port, Error ** err);

int unix_server(const char * path, Error ** err);

#ifdef __cplusplus
}
#endif
#endif
