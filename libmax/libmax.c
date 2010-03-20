#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <glib.h>
#include <gnet.h>

#include <serialize.h>
#include <buffer.h>
#include <array.h>
#include <console.h>
#include <max.h>

#define LOCAL		1
#define REMOTE		2

#define DOM			g_quark_from_static_string("libmax")

#define E_CON		1001
#define E_NOCON		1002
#define E_READ		1003
#define E_WRITE		1004
#define E_SER		1005
#define E_SIG		1006
#define E_SYSCALL	1007
#define E_SYNC		1008

int max_asyscall(maxhandle_t * hand, const char * syscall, const char * signature, void * return_value, void ** args)
{
	GError * err = NULL;
	
	//clear the error before executing
	max_clearerror(hand);

	if (hand->sock == NULL)
	{
		//sock not connected
		hand->gerror = g_error_new_literal(DOM, E_NOCON, "Not connected to kernel");
		return -1;
	}

	buffer_t pbuf = aserialize(method_params(signature), args);
	buffer_t buf = serialize("cssx", T_METHOD, syscall, signature, pbuf);
	if (pbuf == NULL || buf == NULL)
	{
		hand->gerror = g_error_new_literal(DOM, E_SER, "Error during message serialization");
		buffer_free(pbuf);
		buffer_free(buf);
		return -1;
	}
	buffer_free(pbuf);

	gsize len = buffer_size(buf);
	gsize sent, read;
	GIOStatus status;
	
	status = g_io_channel_write_chars((GIOChannel *)hand->sock_io, (char *)buf, len, &sent, &err);
	buffer_free(buf);

	if (status != G_IO_STATUS_NORMAL || err != NULL || sent != len)
	{
		hand->gerror = g_error_new(DOM, E_WRITE, "Could not write to IO stream: %s", (err == NULL)? "??" : err->message);

		if (err != NULL)
		{
			g_error_free(err);
		}
		return -1;
	}

	//receive response
	//switching from buffer_* mem functions to malloc/realloc/free functions
	buf = g_malloc0(sizeof(size_t)); //receive size first

	status = g_io_channel_read_chars((GIOChannel *)hand->sock_io, (char *)buf, sizeof(size_t), &read, &err);
	if (status != G_IO_STATUS_NORMAL || err != NULL || read != sizeof(size_t))
	{
		hand->gerror = g_error_new(DOM, E_READ, "Could not read from IO stream: %s", (err == NULL)? "??" : err->message);

		if (err != NULL)
		{
			g_error_free(err);
		}
		g_free(buf);
		return -1;
	}

	buf = g_realloc(buf, *(size_t *)buf); //now receive rest
	len = buffer_size(buf);

	status = g_io_channel_read_chars((GIOChannel *)hand->sock_io, (char *)buf+sizeof(size_t), len-sizeof(size_t), &read, &err);
	if (status != G_IO_STATUS_NORMAL || err != NULL || read != len-sizeof(size_t))
	{
		hand->gerror = g_error_new(DOM, E_READ, "Could not read from IO stream: %s", (err == NULL)? "??" : err->message);

		if (err != NULL)
		{
			g_error_free(err);
		}
		g_free(buf);
		return -1;
	}

	//deserialize
	void ** arg_array = g_malloc0(param_arraysize("cssx"));
	if (!deserialize("cssx", arg_array, buf))
	{
		//serialize error
		hand->gerror = g_error_new_literal(DOM, E_SER, "Error during deserialization");
		goto done;
	}

	char type = *(char *)arg_array[0];
	char * name = *(char **)arg_array[1];
	char * sig = *(char **)arg_array[2];
	buffer_t params = *(buffer_t *)arg_array[3];

	if (strcmp(name, syscall) != 0)
	{
		//did not respond with correct name (sync error)
		hand->gerror = g_error_new(DOM, E_SYNC, "Got response to other syscall! (given: %s, gotten: %s)", syscall, name);
		goto done;
	}

	switch (type)
	{
		case T_RETURN:
		{
			if (return_value == NULL)
				break;

			switch (sig[0])
			{
				#define __max_asyscall_elem(t1, t2) \
					case t1: { \
						*(t2 *)return_value = *(t2 *)buffer_data(params); \
						break; }

				__max_asyscall_elem(T_BOOLEAN, boolean)
				__max_asyscall_elem(T_INTEGER, int)
				__max_asyscall_elem(T_DOUBLE, double)
				__max_asyscall_elem(T_CHAR, char)

				case T_STRING:
				{
					char * s = (gchar *)buffer_data(params);
					size_t l = strlen(s)+1;
					char * v = malloc(l);
					strncpy(v, s, l);
					*(char **)return_value = v;
					break;
				}

				case T_ARRAY_BOOLEAN:
				case T_ARRAY_INTEGER:
				case T_ARRAY_DOUBLE:
				case T_BUFFER:
				{
					buffer_t v = buffer_copy((buffer_t)buffer_data(params));
					*(buffer_t *)return_value = v;
					break;
				}
			}

			break;
		}

		case T_ERROR:
		{
			hand->gerror = g_error_new(DOM, E_SYSCALL, "Syscall returned an error: %s", (char *)buffer_data(params));
			break;
		}
	}

done:
	g_free(arg_array);
	g_free(buf);
	
	return (hand->gerror == NULL)? 0 : -1;
}

int max_vsyscall(maxhandle_t * hand, const char * syscall, const char * signature, void * return_value, va_list args)
{
	max_clearerror(hand);

	const char * sig = method_params(signature);
	void ** arg_array = vparam_pack(sig, args);
	int ret = max_asyscall(hand, syscall, signature, return_value, arg_array);
	param_free(arg_array);
	return ret;
}

int max_syscall(maxhandle_t * hand, const char * syscall, const char * signature, void * return_value, ...)
{
	va_list args;
	va_start(args, return_value);
	int ret = max_vsyscall(hand, syscall, signature, return_value, args);
	va_end(args);
	return ret;
}

maxhandle_t * max_local()
{
	gnet_init();
	maxhandle_t * hand = g_malloc0(sizeof(maxhandle_t));

	if (!g_file_test(CONSOLE_SOCKFILE, G_FILE_TEST_EXISTS))
	{
		hand->gerror = g_error_new_literal(DOM, E_CON, "Could not connect to local kernel");
		return hand;
	}

	GUnixSocket * sock = gnet_unix_socket_new(CONSOLE_SOCKFILE);
	if (sock == NULL)
	{
		hand->gerror = g_error_new_literal(DOM, E_CON, "Could not connect to local kernel");
		return hand;
	}

	hand->sock_type = LOCAL;
	hand->sock = sock;
	hand->sock_io = gnet_unix_socket_get_io_channel(sock);

	return hand;
}

maxhandle_t * max_remote(const char * host)
{
	gnet_init();
	maxhandle_t * hand = g_malloc0(sizeof(maxhandle_t));

	GTcpSocket * sock = gnet_tcp_socket_connect(host, CONSOLE_TCPPORT);
	if (sock == NULL)
	{
		hand->gerror = g_error_new(DOM, E_CON, "Could not connect to remote kernel (host: %s, port: %d)", host, CONSOLE_TCPPORT);
		return hand;
	}

	gnet_tcp_socket_set_tos(sock, GNET_TOS_LOWDELAY);

	hand->sock_type = REMOTE;
	hand->sock = sock;
	hand->sock_io = gnet_tcp_socket_get_io_channel(sock);

	return hand;
}

void max_close(maxhandle_t * hand)
{
	//TODO - finish me
}


int max_iserror(maxhandle_t * hand)
{
	return hand->gerror != NULL;
}

void max_clearerror(maxhandle_t * hand)
{
	if (hand->gerror != NULL)
	{
		g_error_free((GError *)hand->gerror);
		hand->gerror = NULL;
	}
}

const char * max_error(maxhandle_t * hand)
{
	if (!max_iserror(hand))
		return "no error";

	return ((GError *)hand->gerror)->message;
}


