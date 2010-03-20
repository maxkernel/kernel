#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <aul/common.h>
#include <aul/net.h>
#include <aul/mainloop.h>

#include <console.h>
#include <kernel.h>
#include <serialize.h>

//#define LOCAL		1
//#define NETWORK		2

//static int l_serversock = NULL;	//local socket
//static GTcpSocket * n_serversock = NULL;	//network socket

//MODULE --------------------------------
MOD_INIT(module_init);

gint enable_network = 0;
CFG_PARAM(enable_network, "i");

static String addr2string(uint32_t ip)
{
	unsigned char a4 = (ip & 0xFF000000) >> 24;
	unsigned char a3 = (ip & 0x00FF0000) >> 16;
	unsigned char a2 = (ip & 0x0000FF00) >> 8;
	unsigned char a1 = ip & 0xFF;
	return string_new("%d.%d.%d.%d", a1, a2, a3, a4);
}

static buffer_t mkerror(const char * name, const char * msg)
{
	buffer_t mbuf = serialize(S_STRING, msg);
	buffer_t ebuf = serialize("cssx", T_ERROR, name, "s", mbuf);
	buffer_free(mbuf);
	return ebuf;
}


static void console_replyclient(int fd, buffer_t reply)
{
	ssize_t sent = send(fd, reply, buffer_size(reply), 0);
	if (sent == -1)
	{
		LOG(LOG_WARN, "Error while sending reply: %s", strerror(errno));
	}
	else if (sent != buffer_size(reply))
	{
		LOG(LOG_WARN, "Could not send full reply buffer to client");
	}
}


static boolean console_clientdata(int fd, fdcond_t condition, void * userdata)
{
	ssize_t bytesread;
	size_t bufsize;
	char * buf = NULL;
	void ** array = NULL;

	/*
	char * buf = g_malloc(sizeof(gsize));
	size_t * size = (size_t *)buf;
	void ** array = NULL;
	*/

	//receive length of buffer
	bytesread = recv(fd, &bufsize, sizeof(bufsize), MSG_WAITALL);
	if (bytesread != sizeof(size_t))
	{
		if (bytesread != 0)
		{
			LOG(LOG_WARN, "Could not receive syscall buffer length");
		}

		//if read == 0, socket has closed
		close(fd);
		return FALSE;
	}

	//now receive the rest
	buf = malloc(bufsize);
	memcpy(buf, &bufsize, sizeof(size_t));

	bytesread = recv(fd, buf + sizeof(size_t), bufsize - sizeof(size_t), MSG_WAITALL);
	if (bytesread < bufsize - sizeof(size_t))
	{
		LOG(LOG_WARN, "Could not receive syscall buffer (read=%d, all=%d)", bytesread, bufsize - sizeof(size_t));
		goto done;
	}

	array = malloc0(param_arraysize("cssx"));
	if (!deserialize("cssx", array, (buffer_t)buf))
	{
		LOG(LOG_WARN, "Could not deserialize syscall buffer");
		goto done;
	}

	char type = *(char *)array[0];
	char * name = *(char **)array[1];
	char * sig = *(char **)array[2];
	buffer_t params = *(buffer_t *)array[3];

	switch (type)
	{
		case T_METHOD:
		{
			if (syscall_exists(name, sig))
			{
				const char * param_sig = method_params(sig);
				void ** param_array = malloc0(param_arraysize(param_sig));
				if (!deserialize(param_sig, param_array, params))
				{
					LOG(LOG_WARN, "Could not deserialize parameter buffer with sig '%s'", param_sig);

					buffer_t ebuf = mkerror(name, "Could not deserialize parameter buffer");
					console_replyclient(fd, ebuf);
					buffer_free(ebuf);

					goto done;
				}

				void * ret = asyscall_exec(name, param_array);
				FREE(param_array);

				String rsig = string_new("%c", method_returntype(sig));

				buffer_t pbuf = aserialize(rsig.string, &ret);
				buffer_t rbuf = serialize("cssx", T_RETURN, name, rsig.string, pbuf);
				buffer_free(pbuf);

				console_replyclient(fd, rbuf);
				buffer_free(rbuf);

				//only if return is a buffer, free the memory
				switch (method_returntype(sig))
				{
					case T_ARRAY_BOOLEAN:
					case T_ARRAY_INTEGER:
					case T_ARRAY_DOUBLE:
					case T_BUFFER:
						buffer_free(*(buffer_t *)ret);
						break;

					default:
						break;
				}

				syscall_free(ret);
			}
			else
			{
				String msg = string_new("Syscall %s with signature '%s' doesn't exist!", name, sig);

				buffer_t ebuf = mkerror(name, msg.string);
				console_replyclient(fd, ebuf);
				buffer_free(ebuf);
			}

			break;
		}

		case T_ERROR:
			LOG(LOG_WARN, "Console received an error from client: (%s) %s", name, (char *)buffer_data(params));
			break;

		case T_RETURN:
			LOG(LOG_WARN, "Unexpected return type message received in console (name=%s)", name);
			break;

		default:
			LOG(LOG_WARN, "Unknown message type received in console (name=%s, type=%c)", name, type);
			break;
	}

done:
	FREE(buf);
	FREE(array);

	return TRUE;
}

static boolean console_newunixclient(int fd, fdcond_t condition, void * userdata)
{
	int client = accept(fd, NULL, NULL);
	if (client == -1)
	{
		LOG(LOG_WARN, "Could not accept new UNIX console client socket: %s", strerror(errno));
	}
	else
	{
		mainloop_addwatch(NULL, client, FD_READ, console_clientdata, NULL);
		LOG(LOG_DEBUG, "New UNIX console client (%s)", CONSOLE_SOCKFILE);
	}

	return TRUE;
}

static boolean console_newtcpclient(int fd, fdcond_t condition, void * userdata)
{
	struct sockaddr_in addr;
	size_t size = sizeof(addr);

	int client = accept(fd, (struct sockaddr *)&addr, &size);
	if (client == -1)
	{
		LOG(LOG_WARN, "Could not accept new TCP console client socket: %s", strerror(errno));
	}
	else
	{
		String str_addr = addr2string(addr.sin_addr.s_addr);
		mainloop_addwatch(NULL, client, FD_READ, console_clientdata, NULL);
		LOG(LOG_DEBUG, "New TCP console client (%s)", str_addr.string);
	}
	
	return TRUE;
}

void module_init()
{
	Error * err = NULL;

	LOG(LOG_DEBUG, "Initializing console");

	//start unix socket
	int unixsock = unix_server(CONSOLE_SOCKFILE, &err);
	if (err != NULL)
	{
		LOG(LOG_WARN, "Error creating unix server socket: %s", err->message);
		error_free(err);
	}
	else
	{
		//change permissions on socket (user:rw,group:rw,other:rw)
		chmod(CONSOLE_SOCKFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		mainloop_addwatch(NULL, unixsock, FD_READ, console_newunixclient, NULL);
		LOG(LOG_DEBUG, "Awaiting UNIX console clients on file %s", CONSOLE_SOCKFILE);
	}

	//start network socket (if cfg says to)
	if (enable_network)
	{
		LOG(LOG_DEBUG, "Creating network server socket");

		int tcpsock = tcp_server(CONSOLE_TCPPORT, &err);
		if (err != NULL)
		{
			LOG(LOG_WARN, "Error creating network server socket: %s", err->message);
			error_free(err);
		}
		else
		{
			mainloop_addwatch(NULL, tcpsock, FD_READ, console_newtcpclient, NULL);
			LOG(LOG_DEBUG, "Awaiting TCP console clients on port %d", CONSOLE_TCPPORT);
		}
	}
}

