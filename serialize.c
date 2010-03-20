#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>

#include "array.h"
#include "serialize.h"

#if defined(KERNEL)
  #include "kernel.h"
#elif defined(LIBMAX)
  #include "max.h"
#endif

static size_t param_size(char type, void * data)
{
	switch (type)
	{
		#define __deserialize_elem(t1, t2) \
			case t1: return sizeof(t2);

		__deserialize_elem(T_BOOLEAN, boolean)
		__deserialize_elem(T_INTEGER, int)
		__deserialize_elem(T_DOUBLE, double)
		__deserialize_elem(T_CHAR, char)

		case T_STRING:
			return strlen(*(char **)data)+1;

		case T_BUFFER:
		case T_ARRAY_BOOLEAN:
		case T_ARRAY_INTEGER:
		case T_ARRAY_DOUBLE:
			return buffer_size(*(buffer_t *)data);

		default:
			return 0;
	}
}

buffer_t serialize(const char * sig, ...)
{
	va_list args;
	va_start(args, sig);
	buffer_t buf = vserialize(sig, args);
	va_end(args);

	return buf;
}

buffer_t vserialize(const char * sig, va_list args)
{
	void ** array = vparam_pack(sig, args);
	if (array == NULL)
	{
		#if defined(KERNEL)
			LOG(LOG_WARN, "Could not pack arguments");
		#endif
		return NULL;
	}

	buffer_t buf = aserialize(sig, array);
	param_free(array);

	return buf;
}

buffer_t aserialize(const char * sig, void ** args)
{
	if (args == NULL)
	{
		return buffer_new(0);
	}

	int i;
	size_t len = strlen(sig);
	size_t size = 0;

	for (i=0; i<len; i++)
	{
		size_t s = param_size(sig[i], args[i]);
		if (s == 0 && sig[i] != T_VOID)
		{
			#if defined(KERNEL)
				LOG(LOG_WARN, "Unknown type '%c' in signature '%s'", sig[i], sig);
			#endif
			return NULL;
		}

		size += s;
	}

	buffer_t buf = buffer_new(size);
	char * data = buffer_data(buf);

	for (i=0; i<len; i++)
	{
		size_t s = param_size(sig[i], args[i]);
		switch (sig[i])
		{
			case T_BOOLEAN:
			case T_INTEGER:
			case T_DOUBLE:
			case T_CHAR:
				memcpy(data, args[i], s);
				break;

			case T_STRING:
				strncpy(data, *(char **)args[i], s);
				break;

			case T_BUFFER:
			case T_ARRAY_BOOLEAN:
			case T_ARRAY_INTEGER:
			case T_ARRAY_DOUBLE:
				memcpy(data, *(buffer_t *)args[i], s);
				break;
		}

		data += s;
	}

	return buf;
}

boolean deserialize(const char * sig, void ** params, buffer_t buffer)
{
	int sigi = 0, taili = strlen(sig);
	char * data = buffer_data(buffer);

	for(; sigi<strlen(sig); sigi++)
	{
		switch (sig[sigi])
		{
			case T_VOID:
				break;

			case T_BOOLEAN:
			case T_INTEGER:
			case T_DOUBLE:
			case T_CHAR:
				params[sigi] = data;
				data += param_size(sig[sigi], NULL);
				break;

			case T_STRING:
			case T_BUFFER:
			case T_ARRAY_BOOLEAN:
			case T_ARRAY_INTEGER:
			case T_ARRAY_DOUBLE:
				params[sigi] = params+taili;
				params[taili++] = data;
				data += param_size(sig[sigi], params[sigi]);
				break;

			default:
				#if defined(KERNEL)
					LOG(LOG_WARN, "Invalid type '%c' in signature '%s'", sig[sigi], sig);
				#endif
				return FALSE;
		}

		if (data - (char *)buffer_data(buffer) > buffer_datasize(buffer))
		{
			#if defined(KERNEL)
				LOG(LOG_WARN, "Corrupt buffer");
			#endif
			return FALSE;
		}
	}

	return TRUE;
}

char method_returntype(const char * sig)
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
			return T_VOID;
	}

	#if defined(KERNEL)
		LOG(LOG_WARN, "Unknown return type for method signature '%s'", sig);
	#endif

	return '?';
}

const char * method_params(const char * sig)
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


boolean method_isequal(const char * sig1, const char * sig2)
{
	return strcmp(method_params(sig1), method_params(sig2)) == 0 && method_returntype(sig1) == method_returntype(sig2);
}

size_t param_arraysize(const char * sig)
{
	size_t s = 0;

	int i=0;
	for (; i<strlen(sig); i++)
	{
		switch (sig[i])
		{
			case T_VOID:
				break;

			case T_BOOLEAN:
			case T_INTEGER:
			case T_DOUBLE:
			case T_CHAR:
				s += 1;
				break;

			case T_STRING:
			case T_BUFFER:
			case T_ARRAY_BOOLEAN:
			case T_ARRAY_INTEGER:
			case T_ARRAY_DOUBLE:
				s += 2;
				break;
		}
	}

	return sizeof(void *) * s;
}

void ** param_pack(const char * sig, ...)
{
	va_list args;
	va_start(args, sig);
	void ** array = vparam_pack(sig, args);
	va_end(args);

	return array;
}

void ** vparam_pack(const char * sig, va_list args)
{
	size_t sigi, taili, len;
	len = taili = strlen(sig) * sizeof(void *);

	for(sigi=0; sigi<strlen(sig); sigi++)
	{
		switch (sig[sigi])
		{
			case T_BOOLEAN:	len += sizeof(boolean);		break;
			case T_INTEGER:	len += sizeof(int);			break;
			case T_DOUBLE:	len += sizeof(double);		break;
			case T_CHAR:	len += sizeof(char);		break;

			case T_STRING:
			case T_BUFFER:
			case T_ARRAY_BOOLEAN:
			case T_ARRAY_INTEGER:
			case T_ARRAY_DOUBLE:
				len += sizeof(char *);
				break;

			case T_VOID:
				break;

			default:
				#if defined(KERNEL)
					LOG(LOG_WARN, "Invalid type '%c' in signature '%s'", sig[sigi], sig);
				#endif
				return NULL;
		}
	}

	void ** array = g_malloc0(len);
	for (sigi=0; sigi<strlen(sig); sigi++)
	{
		switch (sig[sigi])
		{
			#define __vparam_pack_elem(t1, t2) \
				case t1: { \
					t2 v = va_arg(args, t2); \
					*(t2 *)(array[sigi] = (char *)array+taili) = v; \
					taili += sizeof(t2); \
					break; }

			__vparam_pack_elem(T_BOOLEAN, boolean)
			__vparam_pack_elem(T_INTEGER, int)
			__vparam_pack_elem(T_DOUBLE, double)

			case T_CHAR:
			{
				char v = (char) va_arg(args, int);
				*(char *)(array[sigi] = (char *)array+taili) = v;
				taili += sizeof(char);
				break;
			}

			__vparam_pack_elem(T_STRING, char *)

			__vparam_pack_elem(T_BUFFER, buffer_t)
			__vparam_pack_elem(T_ARRAY_BOOLEAN, buffer_t)
			__vparam_pack_elem(T_ARRAY_INTEGER, buffer_t)
			__vparam_pack_elem(T_ARRAY_DOUBLE, buffer_t)
		}
	}

	return array;
}

void param_free(void ** array)
{
	g_free(array);
}
