#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include <aul/common.h>

#include <serialize.h>

#if defined(KERNEL)
	#include <kernel.h>
	#include <kernel-priv.h>
#else
	#define LOGK(...)	// Undefine LOGK (we are not compiling in kernel space here
#endif

static size_t headersize(const char * sig)
{
	size_t header_size = 0;
	unsigned int i = 0;

	while (sig[i] != '\0')
	{
		switch (sig[i])
		{
			case T_BOOLEAN:
			case T_INTEGER:
			case T_DOUBLE:
			case T_CHAR:
				header_size += sizeof(void *);
				break;

			case T_STRING:
				header_size += sizeof(char *) + sizeof(char **);
				break;
		}

		i++;
	}

	return header_size;
}

static size_t numparams(const char * sig)
{
	size_t params = 0;
	unsigned int i = 0;

	while (sig[i] != '\0')
	{
		switch(sig[i])
		{
			case T_BOOLEAN:
			case T_INTEGER:
			case T_DOUBLE:
			case T_CHAR:
			case T_STRING:
				params += 1;
				break;
		}

		i++;
	}

	return params;
}

int signature_headerlen(const char * sig)
{
	return headersize(sig);
}

#if defined(KERNEL)
ssize_t serialize_2buffer(buffer_t buffer, const char * sig, ...)
{
	va_list args;
	va_start(args, sig);
	ssize_t ret = vserialize_2buffer(buffer, sig, args);
	va_end(args);

	return ret;
}

ssize_t vserialize_2buffer(buffer_t buffer, const char * sig, va_list args)
{
	unsigned int i = 0;
	ssize_t wrote = 0;

	void copy(const void * ptr, size_t s)
	{
		buffer_write(buffer, ptr, wrote, s);
		wrote += s;
	}

	while (sig[i] != '\0')
	{
		switch (sig[i])
		{
			case T_VOID:
			{
				break;
			}

			case T_BOOLEAN:
			{
				bool v = (bool)va_arg(args, int);
				copy(&v, sizeof(bool));
				break;
			}

			case T_INTEGER:
			{
				int v = va_arg(args, int);
				copy(&v, sizeof(int));
				break;
			}

			case T_DOUBLE:
			{
				double v = va_arg(args, double);
				copy(&v, sizeof(double));
				break;
			}

			case T_CHAR:
			{
				char v = (char)va_arg(args, int);
				copy(&v, sizeof(char));
				break;
			}

			case T_STRING:
			{
				const char * v = va_arg(args, const char *);
				copy(v, strlen(v) + 1);
				break;
			}
		}
		i++;
	}

	return wrote;
}

ssize_t aserialize_2buffer(buffer_t buffer, const char * sig, void ** args)
{
	unsigned int i = 0;
	ssize_t wrote = 0;

	void copy(const void * ptr, size_t s)
	{
		buffer_write(buffer, ptr, wrote, s);
		wrote += s;
	}

	while (sig[i] != '\0')
	{
		switch (sig[i])
		{
			case T_VOID:
			{
				break;
			}

			case T_BOOLEAN:
			{
				copy(args[i], sizeof(bool));
				break;
			}

			case T_INTEGER:
			{
				copy(args[i], sizeof(int));
				break;
			}

			case T_DOUBLE:
			{
				copy(args[i], sizeof(double));
				break;
			}

			case T_CHAR:
			{
				copy(args[i], sizeof(char));
				break;
			}

			case T_STRING:
			{
				const char * str = *(const char **)args[i];
				copy(str, strlen(str) + 1);
				break;
			}

			default:
			{
				LOGK(LOG_ERR, "Could not serialize invalid type (%c) in signature %s", sig[i], sig);
				errno = EINVAL;
				return -1;
			}
		}
		i++;
	}

	return wrote;
}

#endif

ssize_t serialize_2array(void * array, size_t arraylen, const char * sig, ...)
{
	va_list args;
	va_start(args, sig);
	ssize_t ret = vserialize_2array(array, arraylen, sig, args);
	va_end(args);

	return ret;
}

ssize_t vserialize_2array(void * array, size_t arraylen, const char * sig, va_list args)
{
	LABELS(err_nomem);

	unsigned int i = 0;
	ssize_t wrote = 0;

	void copy(const void * ptr, size_t s)
	{
		if ((wrote + s) > arraylen)
		{
			goto err_nomem;
		}
		memcpy((char *)array + wrote, ptr, s);
		wrote += s;
	}

	while (sig[i] != '\0')
	{
		switch (sig[i])
		{
			case T_VOID:
			{
				break;
			}

			case T_BOOLEAN:
			{
				bool v = (bool)va_arg(args, int);
				copy(&v, sizeof(bool));
				break;
			}

			case T_INTEGER:
			{
				int v = va_arg(args, int);
				copy(&v, sizeof(int));
				break;
			}

			case T_DOUBLE:
			{
				double v = va_arg(args, double);
				copy(&v, sizeof(double));
				break;
			}

			case T_CHAR:
			{
				char v = (char)va_arg(args, int);
				copy(&v, sizeof(char));
				break;
			}

			case T_STRING:
			{
				const char * v = va_arg(args, const char *);
				copy(v, strlen(v) + 1);
				break;
			}
		}
		i++;
	}

	return wrote;

err_nomem:
	LOGK(LOG_ERR, "Could not serialize sig %s, array too small!", sig);
	errno = ENOBUFS;
	return -1;
}

ssize_t aserialize_2array(void * array, size_t arraylen, const char * sig, void ** args)
{
	LABELS(err_nomem);

	unsigned int i = 0;
	ssize_t wrote = 0;

	void copy(const void * ptr, size_t s)
	{
		if ((wrote + s) > arraylen)
		{
			goto err_nomem;
		}
		memcpy((char *)array + wrote, ptr, s);
		wrote += s;
	}

	while (sig[i] != '\0')
	{
		switch (sig[i])
		{
			case T_VOID:
			{
				break;
			}

			case T_BOOLEAN:
			{
				copy(args[i], sizeof(bool));
				break;
			}

			case T_INTEGER:
			{
				copy(args[i], sizeof(int));
				break;
			}

			case T_DOUBLE:
			{
				copy(args[i], sizeof(double));
				break;
			}

			case T_CHAR:
			{
				copy(args[i], sizeof(char));
				break;
			}

			case T_STRING:
			{
				const char * str = *(const char **)args[i];
				copy(str, strlen(str) + 1);
				break;
			}

			default:
			{
				LOGK(LOG_ERR, "Could not serialize invalid type (%c) in signature %s", sig[i], sig);
				errno = EINVAL;
				return -1;
			}
		}
		i++;
	}

	return wrote;

err_nomem:
	LOGK(LOG_ERR, "Could not serialize sig %s, array too small!", sig);
	errno = ENOBUFS;
	return -1;
}

ssize_t serialize_2array_wheader(void ** array, size_t arraylen, const char * sig, ...)
{
	va_list args;
	va_start(args, sig);
	ssize_t ret = vserialize_2array_wheader(array, arraylen, sig, args);
	va_end(args);

	return ret;
}

ssize_t vserialize_2array_wheader(void ** array, size_t arraylen, const char * sig, va_list args)
{
	// Get the header size
	size_t header_size = headersize(sig);
	if (header_size > arraylen)
	{
		LOGK(LOG_ERR, "Could not serialize sig %s, array too small!", sig); \
		errno = ENOBUFS;
		return -1;
	}

	// Create the header bit first
	void ** head = array;
	void ** ptrs = head + numparams(sig);
	char * body = (char *)head + header_size;

	unsigned int i = 0, p = 0;
	size_t length = 0;

	va_list cargs;
	va_copy(cargs, args);

	while (sig[i] != '\0')
	{
		head[i] = &body[length];

		switch (sig[i])
		{
			case T_VOID:
			{
				break;
			}

			case T_BOOLEAN:
			{
				va_arg(cargs, int);
				length += sizeof(bool);
				break;
			}

			case T_INTEGER:
			{
				va_arg(cargs, int);
				length += sizeof(int);
				break;
			}

			case T_DOUBLE:
			{
				va_arg(cargs, double);
				length += sizeof(double);
				break;
			}

			case T_CHAR:
			{
				va_arg(cargs, int);
				length += sizeof(char);
				break;
			}

			case T_STRING:
			{
				head[i] = &ptrs[p];
				ptrs[p] = &body[length];
				p++;

				const char * str = va_arg(cargs, const char *);
				length += strlen(str) + 1;

				break;
			}

			default:
			{
				LOGK(LOG_ERR, "Could not serialize invalid type (%c) in signature %s", sig[i], sig);
				errno = EINVAL;
				return -1;
			}
		}

		i++;
	}

	va_end(cargs);

	ssize_t ret = vserialize_2array(body, arraylen-header_size, sig, args);
	if (ret != length)
	{
		LOGK(LOG_ERR, "Header serialization mismatch. Expected body of length %zd, got %zd", length, ret);
		return ret;
	}

	return ret + header_size;
}

ssize_t aserialize_2array_wheader(void ** array, size_t arraylen, const char * sig, void ** args)
{
	// Get the header size
	size_t header_size = headersize(sig);
	if (header_size > arraylen)
	{
		LOGK(LOG_ERR, "Could not serialize sig %s, array too small!", sig); \
		errno = ENOBUFS;
		return -1;
	}

	// Create the header bit first
	void ** head = array;
	void ** ptrs = head + numparams(sig);
	char * body = (char *)head + header_size;

	unsigned int i = 0, p = 0;
	size_t length = 0;

	while (sig[i] != '\0')
	{
		head[i] = &body[length];

		switch (sig[i])
		{
			case T_VOID:
			{
				break;
			}

			case T_BOOLEAN:
			{
				length += sizeof(bool);
				break;
			}

			case T_INTEGER:
			{
				length += sizeof(int);
				break;
			}

			case T_DOUBLE:
			{
				length += sizeof(double);
				break;
			}

			case T_CHAR:
			{
				length += sizeof(char);
				break;
			}

			case T_STRING:
			{
				head[i] = &ptrs[p];
				ptrs[p] = &body[length];
				p++;

				const char * str = *(const char **)args[i];
				length += strlen(str) + 1;

				break;
			}

			default:
			{
				LOGK(LOG_ERR, "Could not serialize invalid type (%c) in signature %s", sig[i], sig);
				errno = EINVAL;
				return -1;
			}
		}

		i++;
	}

	ssize_t ret = aserialize_2array(body, arraylen-header_size, sig, args);
	if (ret != length)
	{
		LOGK(LOG_ERR, "Header serialization mismatch. Expected body of length %zd, got %zd", length, ret);
		return ret;
	}

	return ret + header_size;
}

ssize_t serialize_2array_fromcb(void * array, size_t arraylen, const char * sig, args_f args)
{

	unsigned int i = 0;
	ssize_t wrote = 0;

	while (sig[i] != '\0')
	{
		switch (sig[i])
		{
			case T_VOID:
			{
				break;
			}

			case T_BOOLEAN:
			case T_INTEGER:
			case T_DOUBLE:
			case T_CHAR:
			case T_STRING:
			{
				exception_t * err = NULL;
				ssize_t w = args((char *)array + wrote, arraylen - wrote, &err, sig, i);
				if (exception_check(&err) || w <= 0)
				{
					LOGK(LOG_ERR, "Could not serialize index %d (type %c): %s", i, sig[i], err == NULL? "Unknown error" : err->message);
					errno = EINVAL;
					return -1;
				}

				wrote += w;
				break;
			}

			default:
			{
				LOGK(LOG_ERR, "Could not serialize invalid type (%c) in signature %s", sig[i], sig);
				errno = EINVAL;
				return -1;
			}
		}

		if (wrote > arraylen)
		{
			// Buffer overflow
			LOGK(LOG_ERR, "Could not serialize sig %s, body array too small!", sig);
			errno = ENOBUFS;
			return -1;
		}

		i++;
	}

	return wrote;
}

ssize_t serialize_2array_fromcb_wheader(void ** array, size_t arraylen, const char * sig, args_f args)
{
	// Get the header size
	size_t header_size = headersize(sig);
	if (header_size > arraylen)
	{
		LOGK(LOG_ERR, "Could not serialize sig %s, array too small!", sig); \
		errno = ENOBUFS;
		return -1;
	}

	// Create the header bit first
	void ** head = array;
	void ** ptrs = head + numparams(sig);
	char * body = (char *)head + header_size;

	unsigned int i = 0, p = 0;
	ssize_t wrote = 0;

	while (sig[i] != '\0')
	{
		head[i] = &body[wrote];

		switch (sig[i])
		{
			case T_VOID:
			{
				break;
			}

			case T_STRING:
			{
				head[i] = &ptrs[p];
				ptrs[p] = &body[wrote];
				p++;

				// No break here, we want to fall into the next case statement
			}

			case T_BOOLEAN:
			case T_INTEGER:
			case T_DOUBLE:
			case T_CHAR:
			{
				exception_t * err = NULL;
				ssize_t w = args((char *)body + wrote, arraylen - header_size - wrote, &err, sig, i);
				if (exception_check(&err) || w <= 0)
				{
					LOGK(LOG_ERR, "Could not serialize index %d (type %c): %s", i, sig[i], err == NULL? "Unknown error" : err->message);
					errno = EINVAL;
					return -1;
				}

				wrote += w;
				break;
			}

			default:
			{
				LOGK(LOG_ERR, "Could not serialize invalid type (%c) in signature %s", sig[i], sig);
				errno = EINVAL;
				return -1;
			}
		}

		if (wrote > (arraylen - header_size))
		{
			// Buffer overflow
			LOGK(LOG_ERR, "Could not serialize sig %s, body array too small!", sig);
			errno = ENOBUFS;
			return -1;
		}

		i++;
	}

	return header_size + wrote;
}

ssize_t deserialize_2args(const char * sig, void * array, size_t arraylen, ...)
{
	LABELS(err_nomem);

	va_list args;
	va_start(args, arraylen);

	unsigned int i = 0;
	size_t length = 0;

	void copy(void * ptr, size_t s)
	{
		if ((length + s) > arraylen)
		{
			goto err_nomem;
		}
		memcpy(ptr, (char *)array + length, s);
		length += s;
	}

	while (sig[i] != '\0')
	{
		switch (sig[i])
		{
			case T_VOID:
			{
				break;
			}

			case T_BOOLEAN:
			{
				bool * ptr = va_arg(args, bool *);
				copy(ptr, sizeof(bool));
				break;
			}

			case T_INTEGER:
			{
				int * ptr = va_arg(args, int *);
				copy(ptr, sizeof(int));
				break;
			}

			case T_DOUBLE:
			{
				double * ptr = va_arg(args, double *);
				copy(ptr, sizeof(double));
				break;
			}

			case T_CHAR:
			{
				char * ptr = va_arg(args, char *);
				copy(ptr, sizeof(char));
				break;
			}

			case T_STRING:
			{
				char ** ptr = va_arg(args, char **);
				*ptr = (char *)array + length;
				length += strlen(*ptr) + 1;

				if (length > arraylen)
				{
					goto err_nomem;
				}

				break;
			}

			default:
			{
				LOGK(LOG_ERR, "Could not deserialize invalid type (%c) in signature %s", sig[i], sig);
				errno = EINVAL;
				return -1;
			}
		}

		i++;
	}

	va_end(args);

	return length;

err_nomem:
	LOGK(LOG_ERR, "Could not deserialize sig %s, array too small!", sig);
	errno = ENOBUFS;
	return -1;
}

ssize_t deserialize_2header(void ** header, size_t headerlen, const char * sig, void * array, size_t arraylen)
{
	LABELS(err_nomem);

	// Get the header size
	size_t header_size = headersize(sig);
	if (header_size > headerlen)
	{
		LOGK(LOG_ERR, "Could not deserialize sig %s, header too small!", sig);
		errno = ENOBUFS;
		return -1;
	}

	// Create the header bit first
	void ** ptrs = header + numparams(sig);

	unsigned int i = 0, p = 0;
	size_t length = 0;

	while (sig[i] != '\0')
	{
		header[i] = (char *)array + length;

		switch (sig[i])
		{
			case T_VOID:
			{
				break;
			}

			case T_BOOLEAN:
			{
				length += sizeof(bool);
				break;
			}

			case T_INTEGER:
			{
				length += sizeof(int);
				break;
			}

			case T_DOUBLE:
			{
				length += sizeof(double);
				break;
			}

			case T_CHAR:
			{
				length += sizeof(char);
				break;
			}

			case T_STRING:
			{
				header[i] = &ptrs[p];
				ptrs[p] = (char *)array + length;
				p++;

				const char * str = *(const char **)header[i];
				length += strlen(str) + 1;

				break;
			}

			default:
			{
				LOGK(LOG_ERR, "Could not deserialize invalid type (%c) in signature %s", sig[i], sig);
				errno = EINVAL;
				return -1;
			}
		}

		if (length > arraylen)
		{
			goto err_nomem;
		}

		i++;
	}

	return length;

err_nomem:
	LOGK(LOG_ERR, "Could not deserialize sig %s, array too small!", sig);
	errno = ENOBUFS;
	return -1;
}

#if defined(KERNEL)

ssize_t deserialize_2header_wbody(void ** header, size_t headerlen, const char * sig, buffer_t buffer)		// TODO - This function sucks! Make it better
{
	LABELS(err_nomem, err_bufsize);

	// Get the header size
	size_t header_size = headersize(sig);
	if (header_size > headerlen)
	{
		goto err_nomem;
	}

	char * body = (char *)header + header_size;

	unsigned int i = 0;
	size_t read = 0;

	void copy(size_t s)
	{
		if ((header_size + read + s) > headerlen)
		{
			goto err_nomem;
		}

		size_t r = buffer_read(buffer, &body[read], read, s);
		if (r != s)
		{
			goto err_bufsize;
		}

		read += s;
	}

	while (sig[i] != '\0')
	{
		switch (sig[i])
		{
			case T_VOID:
			{
				break;
			}

			case T_BOOLEAN:
			{
				copy(sizeof(bool));
				break;
			}

			case T_INTEGER:
			{
				copy(sizeof(int));
				break;
			}

			case T_DOUBLE:
			{
				copy(sizeof(double));
				break;
			}

			case T_CHAR:
			{
				copy(sizeof(char));
				break;
			}

			case T_STRING:
			{
				char byte;
				do
				{
					size_t r = buffer_read(buffer, &byte, read, sizeof(byte));
					if (r != sizeof(byte))
					{
						goto err_bufsize;
					}

					if ((header_size + read + sizeof(byte)) > headerlen)
					{
						goto err_nomem;
					}

					body[read++] = byte;

				} while (byte != '\0');
				break;
			}

			default:
			{
				LOGK(LOG_ERR, "Could not serialize invalid type (%c) in signature %s", sig[i], sig);
				errno = EINVAL;
				return -1;
			}
		}

		i++;
	}

	ssize_t ret = deserialize_2header(header, header_size, sig, body, read);
	if (ret != header_size)
	{
		LOGK(LOG_ERR, "Header deserialization mismatch. Expected header of length %zd, got %zd", header_size, ret);
		return ret;
	}

	return header_size + read;

err_nomem:
	LOGK(LOG_ERR, "Could not deserialize sig %s, array too small!", sig);
	errno = ENOBUFS;
	return -1;

err_bufsize:
	LOGK(LOG_ERR, "Could not deserialize sig %s, buffer length error!", sig);
	errno = ENOSPC;
	return -1;
}

#endif

/* ------------------------------------------------------------------- */
#if 0
static size_t param_size(char type, void * data)
{
	switch (type)
	{
		case T_BOOLEAN:	return sizeof(bool);
		case T_INTEGER:	return sizeof(int);
		case T_DOUBLE:	return sizeof(double);
		case T_CHAR:	return sizeof(char);
		case T_STRING:	return strlen(*(char **)data)+1;
		default:		return 0;
	}
}

void serialize(buffer_t buffer, const char * sig, ...)
{
	va_list args;
	va_start(args, sig);
	vserialize(buffer, sig, args);
	va_end(args);
}

void vserialize(buffer_t buffer, const char * sig, va_list args)
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

bool deserialize(const char * sig, void ** params, buffer_t buffer)
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
				return false;
		}

		if (data - (char *)buffer_data(buffer) > buffer_datasize(buffer))
		{
			#if defined(KERNEL)
				LOG(LOG_WARN, "Corrupt buffer");
			#endif
			return false;
		}
	}

	return true;
}

void param_pack(void ** buffer, size_t buflen, const char * sig, ...)
{
	va_list args;
	va_start(args, sig);
	ssize_t ret = vparam_pack(buffer, buflen, sig, args);
	va_end(args);

	return ret;
}

ssize_t vparam_pack(void ** buffer, size_t buflen, const char * sig, va_list args)
{
	// Sanity check
	{
		if (buffer == NULL || sig == NULL)
		{
			LOGK(LOG_ERR, "Could not pack params with NULL buffer or NULL sig!");
			return;
		}

		unsigned int i = 0;
		while (sig[i] != '\0')
		{
			switch (sig[i])
			{
				case T_BOOLEAN:
				case T_INTEGER:
				case T_DOUBLE:
				case T_CHAR:
				case T_STRING:
					// These are allowed
					break;

				default:
					// Everything is not allowed
					LOGK(LOG_ERR, "Could not pack invalid parameter %d of sig %s", i+1, sig);
					return;
			}

			i++;
		}
	}

	// Calculate the header size
	size_t header_size = 0;
	{
		unsigned int i = 0;
		while (sig[i] != '\0')
		{
			switch (sig[i])
			{
				case T_BOOLEAN:
				case T_INTEGER:
				case T_DOUBLE:
				case T_CHAR:
					header_size += sizeof(void *);
					break;

				case T_STRING:
					header_size += sizeof(char *) + sizeof(char **);
					break;
			}

			i++;
		}

		if (header_size > buflen)
		{
			errno = ENOBUFS;
			return -1;
		}
	}

	// Pack the params into the given array
	char * header = (char *)buffer;
	char * ptrs = header + (strlen(sig) * sizeof(void *));
	char * body = header + header_size;
	size_t bufsize = header_size;

#define __vparam_pack_checklen(sz) if ((bufsize + (sz)) > buflen) { errno = ENOBUFS; return -1; }

	while (*sig != '\0')
	{
		switch (*sig)
		{
			case T_BOOLEAN:
			{
				__vparam_pack_checklen(sizeof(bool))
				bool v = (bool)va_arg(args, int);

				memcpy(body, &v, sizeof(bool));
				*(bool **)header = &*(bool *)body;

				body += sizeof(bool);
				header += sizeof(bool *);

				break;
			}

			case T_INTEGER:
			{
				__vparam_pack_checklen(sizeof(int))
				int v = va_arg(args, int);

				memcpy(body, &v, sizeof(int));
				*(int **)header = &*(int *)body;

				body += sizeof(int);
				header += sizeof(int *);

				break;
			}

			case T_DOUBLE:
			{
				__vparam_pack_checklen(sizeof(double))
				double v = va_arg(args, double);

				memcpy(body, &v, sizeof(double));
				*(double **)header = &*(double *)body;

				body += sizeof(double);
				header += sizeof(double *);

				break;
			}

			case T_CHAR:
			{
				__vparam_pack_checklen(sizeof(char))
				char v = (char)va_arg(args, int);

				memcpy(body, &v, sizeof(char));
				*(char **)header = &*(char *)body;

				body += sizeof(char);
				header += sizeof(char *);

				break;
			}

			case T_STRING:
			{
				char * v = va_arg(args, char *);
				__vparam_pack_checklen(strlen(v)+1)

				memcpy(body, v, strlen(v)+1);
				*(char ***)header = &*(char **)ptrs;
				*(char **)ptrs = &*(char *)body;

				body += strlen(v)+1;
				header += sizeof(char **);
				ptrs += sizeof(char *);

				break;
			}
		}

		sig++;
	}

	return body - (char *)buffer;
}
#endif
