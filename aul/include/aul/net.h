#ifndef __AUL_NET_H
#define __AUL_NET_H

#include <inttypes.h>
#include <aul/exception.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUL_NET_BACKLOG		2

int udp_server(uint16_t port, exception_t ** err);

int tcp_server(uint16_t port, exception_t ** err);

int unix_server(const char * path, exception_t ** err);

#ifdef __cplusplus
}
#endif
#endif
