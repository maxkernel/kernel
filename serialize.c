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
