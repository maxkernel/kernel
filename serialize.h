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
#include <kernel-types.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline char method_returntype(const char * sig)
{
	switch (sig[0])
	{
		case T_BOOLEAN:
		case T_INTEGER:
		case T_DOUBLE:
		case T_CHAR:
		case T_STRING:
		case T_BUFFER:
		case T_ARRAY_BOOLEAN:
		case T_ARRAY_INTEGER:
		case T_ARRAY_DOUBLE:
		case T_VOID:
			return sig[0];

		case ':':
		case '\0':
			return T_VOID;
	}

	return '?';
}

static inline const char * method_params(const char * sig)
{
	const char * ptr;
	if ((ptr = strchr(sig, ':')) == NULL)
	{
		return sig;
	}

	if (*(ptr+1) == '\0')
	{
		return "v";
	}

	return ptr+1;
}

static inline bool method_isequal(const char * sig1, const char * sig2)
{
	return strcmp(method_params(sig1), method_params(sig2)) == 0 && method_returntype(sig1) == method_returntype(sig2);
}

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
