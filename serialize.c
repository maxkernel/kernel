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
#endif

static size_t headersize(const char * sig)
{
	size_t header_size = 0;

	const char * param = NULL;
	method_foreachparam(param, sig)
	{
		switch (*param)
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

			default:
				break;
		}
	}

	return header_size;
}

static size_t numparams(const char * sig)
{
	size_t params = 0;

	const char * param = NULL;
	method_foreachparam(param, sig)
	{
		switch(*param)
		{
			case T_BOOLEAN:
			case T_INTEGER:
			case T_DOUBLE:
			case T_CHAR:
			case T_STRING:
				params += 1;
				break;

			default:
				break;
		}
	}

	return params;
}

int signature_headerlen(const char * sig)
{
	return headersize(sig);
}

#if defined(KERNEL)
ssize_t serialize_2buffer(buffer_t buffer, exception_t ** err, const char * sig, ...)
{
	va_list args;
	va_start(args, sig);
	ssize_t ret = vserialize_2buffer(buffer, err, sig, args);
	va_end(args);

	return ret;
}

ssize_t vserialize_2buffer(buffer_t buffer, exception_t ** err, const char * sig, va_list args)
{
	ssize_t wrote = 0;

	void copy(const void * ptr, size_t s)
	{
		buffer_write(buffer, ptr, wrote, s);
		wrote += s;
	}

	const char * param = NULL;
	method_foreachparam(param, sig)
	{
		switch (*param)
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

			default:
			{
				exception_set(err, EINVAL, "Could not serialize invalid type '%c' in sig %s", *param, sig);
				return -1;
			}
		}
	}

	return wrote;
}

ssize_t aserialize_2buffer(buffer_t buffer, exception_t ** err, const char * sig, void ** args)
{
	size_t index = 0;
	ssize_t wrote = 0;

	void copy(const void * ptr, size_t s)
	{
		buffer_write(buffer, ptr, wrote, s);
		wrote += s;
	}

	const char * param = NULL;
	method_foreachparam(param, sig)
	{
		switch (*param)
		{
			case T_VOID:
			{
				break;
			}

			case T_BOOLEAN:
			{
				copy(args[index], sizeof(bool));
				break;
			}

			case T_INTEGER:
			{
				copy(args[index], sizeof(int));
				break;
			}

			case T_DOUBLE:
			{
				copy(args[index], sizeof(double));
				break;
			}

			case T_CHAR:
			{
				copy(args[index], sizeof(char));
				break;
			}

			case T_STRING:
			{
				const char * str = *(const char **)args[index];
				copy(str, strlen(str) + 1);
				break;
			}

			default:
			{
				exception_set(err, EINVAL, "Could not serialize invalid type '%c' in sig %s", *param, sig);
				return -1;
			}
		}

		index += 1;
	}

	return wrote;
}

#endif

ssize_t serialize_2array(void * array, size_t arraylen, exception_t ** err, const char * sig, ...)
{
	va_list args;
	va_start(args, sig);
	ssize_t ret = vserialize_2array(array, arraylen, err, sig, args);
	va_end(args);

	return ret;
}

ssize_t vserialize_2array(void * array, size_t arraylen, exception_t ** err, const char * sig, va_list args)
{
	labels(err_nomem);

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

	const char * param = NULL;
	method_foreachparam(param, sig)
	{
		switch (*param)
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

			default:
			{
				exception_set(err, EINVAL, "Could not serialize invalid type '%c' in sig %s", *param, sig);
				return -1;
			}
		}
	}

	return wrote;

err_nomem:
	exception_set(err, ENOBUFS, "Could not serialize sig %s, array too small!", sig);
	return -1;
}

ssize_t aserialize_2array(void * array, size_t arraylen, exception_t ** err, const char * sig, void ** args)
{
	labels(err_nomem);

	size_t index = 0;
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

	const char * param = NULL;
	method_foreachparam(param, sig)
	{
		switch (*param)
		{
			case T_VOID:
			{
				break;
			}

			case T_BOOLEAN:
			{
				copy(args[index], sizeof(bool));
				break;
			}

			case T_INTEGER:
			{
				copy(args[index], sizeof(int));
				break;
			}

			case T_DOUBLE:
			{
				copy(args[index], sizeof(double));
				break;
			}

			case T_CHAR:
			{
				copy(args[index], sizeof(char));
				break;
			}

			case T_STRING:
			{
				const char * str = *(const char **)args[index];
				copy(str, strlen(str) + 1);
				break;
			}

			default:
			{
				exception_set(err, EINVAL, "Could not serialize invalid type '%c' in sig %s", *param, sig);
				return -1;
			}
		}

		index += 1;
	}

	return wrote;

err_nomem:
	exception_set(err, ENOBUFS, "Could not serialize sig %s, array too small!", sig);
	return -1;
}

ssize_t serialize_2array_wheader(void ** array, size_t arraylen, exception_t ** err, const char * sig, ...)
{
	va_list args;
	va_start(args, sig);
	ssize_t ret = vserialize_2array_wheader(array, arraylen, err, sig, args);
	va_end(args);

	return ret;
}

ssize_t vserialize_2array_wheader(void ** array, size_t arraylen, exception_t ** err, const char * sig, va_list args)
{
	// Get the header size
	size_t header_size = headersize(sig);
	if (header_size > arraylen)
	{
		exception_set(err, ENOBUFS, "Could not serialize sig %s, array too small!", sig);
		return -1;
	}

	// Create the header bit first
	void ** head = array;
	void ** ptrs = head + numparams(sig);
	char * body = (char *)head + header_size;

	size_t index = 0, pindex = 0;
	ssize_t length = 0;

	va_list cargs;
	va_copy(cargs, args);
	{
		const char * param = NULL;
		method_foreachparam(param, sig)
		{
			head[index] = &body[length];

			switch (*param)
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
					head[index] = &ptrs[pindex];
					ptrs[pindex] = &body[length];
					pindex += 1;

					const char * str = va_arg(cargs, const char *);
					length += strlen(str) + 1;

					break;
				}

				default:
				{
					exception_set(err, EINVAL, "Could not serialize invalid type '%c' in sig %s", *param, sig);
					return -1;
				}
			}

			index += 1;
		}
	}
	va_end(cargs);

	ssize_t ret = vserialize_2array(body, arraylen-header_size, err, sig, args);
	if (ret != length)
	{
		if (!exception_check(err))
		{
			exception_set(err, EFAULT, "Header serialization mismatch. Expected body of length %zd, got %zd", length, ret);
		}

		return -1;
	}

	return ret + header_size;
}

ssize_t aserialize_2array_wheader(void ** array, size_t arraylen, exception_t ** err, const char * sig, void ** args)
{
	// Get the header size
	size_t header_size = headersize(sig);
	if (header_size > arraylen)
	{
		exception_set(err, ENOBUFS, "Could not serialize sig %s, array too small!", sig);
		return -1;
	}

	// Create the header bit first
	void ** head = array;
	void ** ptrs = head + numparams(sig);
	char * body = (char *)head + header_size;

	size_t index = 0, pindex = 0;
	ssize_t length = 0;

	const char * param = NULL;
	method_foreachparam(param, sig)
	{
		head[index] = &body[length];

		switch (*param)
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
				head[index] = &ptrs[pindex];
				ptrs[pindex] = &body[length];
				pindex += 1;

				const char * str = *(const char **)args[index];
				length += strlen(str) + 1;

				break;
			}

			default:
			{
				exception_set(err, EINVAL, "Could not serialize invalid type '%c' in sig %s", *param, sig);
				return -1;
			}
		}

		index += 1;
	}

	ssize_t ret = aserialize_2array(body, arraylen-header_size, err, sig, args);
	if (ret != length)
	{
		if (!exception_check(err))
		{
			exception_set(err, EFAULT, "Header serialization mismatch. Expected body of length %zd, got %zd", length, ret);
		}

		return -1;
	}

	return ret + header_size;
}

ssize_t serialize_2array_fromcb(void * array, size_t arraylen, exception_t ** err, const char * sig, args_f args)
{
	size_t index = 0;
	ssize_t wrote = 0;

	const char * param = NULL;
	method_foreachparam(param, sig)
	{
		switch (*param)
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
				ssize_t w = args((char *)array + wrote, arraylen - wrote, err, sig, index);
				if (exception_check(err) || w <= 0)
				{
					return -1;
				}

				wrote += w;
				break;
			}

			default:
			{
				exception_set(err, EINVAL, "Could not serialize invalid type '%c' in sig %s", *param, sig);
				return -1;
			}
		}

		if ((size_t)wrote > arraylen)
		{
			// Buffer overflow
			exception_set(err, ENOBUFS, "Could not serialize sig %s, body array too small!", sig);
			return -1;
		}

		index += 1;
	}

	return wrote;
}

ssize_t serialize_2array_fromcb_wheader(void ** array, size_t arraylen, exception_t ** err, const char * sig, args_f args)
{
	// Get the header size
	size_t header_size = headersize(sig);
	if (header_size > arraylen)
	{
		exception_set(err, ENOBUFS, "Could not serialize sig %s, array too small!", sig);
		return -1;
	}

	// Create the header bit first
	void ** head = array;
	void ** ptrs = head + numparams(sig);
	char * body = (char *)head + header_size;

	size_t index = 0, pindex = 0;
	ssize_t wrote = 0;

	const char * param = NULL;
	method_foreachparam(param, sig)
	{
		head[index] = &body[wrote];

		switch (*param)
		{
			case T_VOID:
			{
				break;
			}

			case T_STRING:
			{
				head[index] = &ptrs[pindex];
				ptrs[pindex] = &body[wrote];
				pindex += 1;

				// No break here, we want to fall into the next case statement
			}

			case T_BOOLEAN:
			case T_INTEGER:
			case T_DOUBLE:
			case T_CHAR:
			{
				ssize_t w = args((char *)body + wrote, arraylen - header_size - wrote, err, sig, index);
				if (exception_check(err) || w <= 0)
				{
					return -1;
				}

				wrote += w;
				break;
			}

			default:
			{
				exception_set(err, EINVAL, "Could not serialize invalid type '%c' in sig %s", *param, sig);
				return -1;
			}
		}

		if ((size_t)wrote > (arraylen - header_size))
		{
			// Buffer overflow
			exception_set(err, ENOBUFS, "Could not serialize sig %s, body array too small!", sig);
			return -1;
		}

		index += 1;
	}

	return header_size + wrote;
}

ssize_t deserialize_2args(void * array, size_t arraylen, exception_t ** err, const char * sig, ...)
{
	labels(err_nomem);

	va_list args;
	va_start(args, sig);

	size_t wrote = 0;

	void copy(void * ptr, size_t s)
	{
		if ((wrote + s) > arraylen)
		{
			goto err_nomem;
		}
		memcpy(ptr, (char *)array + wrote, s);
		wrote += s;
	}

	const char * param = NULL;
	method_foreachparam(param, sig)
	{
		switch (*param)
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
				*ptr = (char *)array + wrote;
				wrote += strlen(*ptr) + 1;

				if (wrote > arraylen)
				{
					goto err_nomem;
				}

				break;
			}

			default:
			{
				exception_set(err, EINVAL, "Could not deserialize invalid type '%c' in sig %s", *param, sig);
				return -1;
			}
		}
	}

	va_end(args);

	return wrote;

err_nomem:
	exception_set(err, ENOBUFS, "Could not deserialize sig %s, array too small!", sig);
	return -1;
}

ssize_t deserialize_2header(void ** header, size_t headerlen, exception_t ** err, const char * sig, void * array, size_t arraylen)
{
	labels(err_nomem);

	// Get the header size
	size_t header_size = headersize(sig);
	if (header_size > headerlen)
	{
		exception_set(err, ENOBUFS, "Could not deserialize sig %s, header too small!", sig);
		return -1;
	}

	// Create the header bit first
	void ** ptrs = header + numparams(sig);

	size_t index = 0, pindex = 0;
	size_t length = 0;

	const char * param = NULL;
	method_foreachparam(param, sig)
	{
		header[index] = (char *)array + length;

		switch (*param)
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
				header[index] = &ptrs[pindex];
				ptrs[pindex] = (char *)array + length;
				pindex += 1;

				const char * str = *(const char **)header[index];
				length += strlen(str) + 1;

				break;
			}

			default:
			{
				exception_set(err, EINVAL, "Could not deserialize invalid type '%c' in sig %s", *param, sig);
				return -1;
			}
		}

		if (length > arraylen)
		{
			goto err_nomem;
		}

		index += 1;
	}

	return length;

err_nomem:
	exception_set(err, ENOBUFS, "Could not deserialize sig %s, array too small!", sig);
	return -1;
}


#if defined(KERNEL)

ssize_t deserialize_2header_wbody(void ** header, size_t headerlen, exception_t ** err, const char * sig, buffer_t buffer)		// TODO - This function sucks! Make it better
{
	labels(err_nomem, err_bufsize);

	// Get the header size
	ssize_t header_size = headersize(sig);
	if ((size_t)header_size > headerlen)
	{
		goto err_nomem;
	}

	char * body = (char *)header + header_size;
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

	size_t index = 0;

	const char * param = NULL;
	method_foreachparam(param, sig)
	{
		switch (*param)
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
				char byte = 0;
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

					body[read] = byte;
					read += 1;

				} while (byte != '\0');
				break;
			}

			default:
			{
				exception_set(err, EINVAL, "Could not serialize invalid type '%c' in sig %s", *param, sig);
				return -1;
			}
		}

		index += 1;
	}

	ssize_t ret = deserialize_2header(header, header_size, err, sig, body, read);
	if (ret != header_size)
	{
		if (!exception_check(err))
		{
			exception_set(err, EFAULT, "Header deserialization mismatch. Expected header of length %zd, got %zd", header_size, ret);
		}

		return -1;
	}

	return header_size + read;

err_nomem:
	exception_set(err, ENOBUFS, "Could not deserialize sig %s, array too small!", sig);
	return -1;

err_bufsize:
	exception_set(err, ENOBUFS, "Could not deserialize sig %s, buffer length error!", sig);
	return -1;
}

#endif
