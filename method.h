#ifndef __METHOD_H
#define __METHOD_H

#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <kernel-types.h>

#ifdef __cplusplus
extern "C" {
#endif


#define method_foreachparam(param, sig) \
	for ((param) = (sig); *(param) != '\0'; (param) += 1)

static inline const char method_returntype(const char * sig)
{
	switch (*sig)
	{
		case ':':
		case '\0':
			return T_VOID;
		
		default:
			return *sig;
	}
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

static inline size_t method_numparams(const char * sig)
{
	size_t params = 0;
	while(*sig != '\0')
	{
		switch(*sig)
		{
			case T_VOID:
				break;

			default:
				params += 1;
				break;
		}

		sig += 1;
	}

	return params;
}

static inline bool method_isequal(const char * sig1, const char * sig2)
{
	return strcmp(method_params(sig1), method_params(sig2)) == 0 && method_returntype(sig1) == method_returntype(sig2);
}

#ifdef __cplusplus
}
#endif
#endif
