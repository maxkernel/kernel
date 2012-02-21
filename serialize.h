#ifndef __SERIALIZE_H
#define __SERIALIZE_H

#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>

#if defined(KERNEL)
	#include <buffer.h>
#endif

#include <aul/exception.h>
#include <method.h>
#include <kernel-types.h>

#ifdef __cplusplus
extern "C" {
#endif

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

typedef ssize_t (*args_f)(void * array, size_t arraylen, exception_t ** e, const char * sig, int index);
ssize_t serialize_2array_fromcb(void * array, size_t arraylen, const char * sig, args_f args);
ssize_t serialize_2array_fromcb_wheader(void ** array, size_t arraylen, const char * sig, args_f args);

ssize_t deserialize_2args(const char * sig, void * array, size_t arraylen, ...);
ssize_t deserialize_2header(void ** header, size_t headerlen, const char * sig, void * array, size_t arraylen);

#if defined(KERNEL)
ssize_t deserialize_2header_wbody(void ** header, size_t headerlen, const char * sig, buffer_t buffer);
#endif

#ifdef __cplusplus
}
#endif
#endif
