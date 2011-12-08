#ifndef __SERIALIZE_H
#define __SERIALIZE_H

#include <stdarg.h>
#include <stdbool.h>
#include <sys/types.h>

#if defined(KERNEL)
	#include <buffer.h>
#endif

#include <kernel-types.h>

#ifdef __cplusplus
extern "C" {
#endif

char method_returntype(const char * sig);
const char * method_params(const char * sig);
bool method_isequal(const char * sig1, const char * sig2);

int signature_headerlen(const char * sig);

#if defined(KERNEL)
ssize_t serialize_2buffer(buffer_t buffer, const char * sig, ...);
ssize_t vserialize_2buffer(buffer_t buffer, const char * sig, va_list args);
ssize_t aserialize_2buffer(buffer_t buffer, const char * sig, void ** args);
#endif

ssize_t serialize_2array(void * array, size_t arraylen, const char * sig, ...);
ssize_t vserialize_2array(void * array, size_t arraylen, const char * sig, va_list args);
ssize_t aserialize_2array(void * array, size_t arraylen, const char * sig, void ** args);

ssize_t serialize_2array_wheader(void ** array, size_t arraylen, const char * sig, ...);
ssize_t vserialize_2array_wheader(void ** array, size_t arraylen, const char * sig, va_list args);
ssize_t aserialize_2array_wheader(void ** array, size_t arraylen, const char * sig, void ** args);

ssize_t deserialize_2args(const char * sig, void * array, size_t arraylen, ...);
ssize_t deserialize_2header(void ** header, size_t headerlen, const char * sig, void * array, size_t arraylen);

#if defined(KERNEL)
ssize_t deserialize_2header_wbody(void ** header, size_t headerlen, const char * sig, buffer_t buffer);
#endif

#if 0
buffer_t serialize(const char * sig, ...);
buffer_t vserialize(const char * sig, va_list args);
buffer_t aserialize(const char * sig, void ** args);

bool deserialize(const char * sig, void ** params, buffer_t buffer);

const char * method_params(const char * sig);
char method_returntype(const char * sig);
bool method_isequal(const char * sig1, const char * sig2);

ssize_t param_pack(void ** buffer, size_t buflen, const char * sig, ...);
ssize_t vparam_pack(void ** buffer, size_t buflen, const char * sig, va_list args);
#endif


#ifdef __cplusplus
}
#endif
#endif
