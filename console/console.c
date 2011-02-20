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
#include <aul/list.h>

#include <console.h>
#include <kernel.h>
#include <serialize.h>

MOD_INIT(module_init);

int enable_network = 0;
CFG_PARAM(enable_network, "i");


typedef struct
{
	list_t free_list;
	size_t size;
	char buffer[CONSOLE_BUFFERMAX];
} cbuffer_t;

static list_t free_buffers;


static string_t addr2string(uint32_t ip)
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

static bool console_clientdata(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	cbuffer_t * buffer = userdata;

	// Get data and put in buffer
	ssize_t bytesread = recv(fd, &buffer->buffer, CONSOLE_BUFFERMAX - buffer->size, 0);
	if (bytesread <= 0)
	{
		// Socket has closed or error'd
		if (bytesread != 0)
		{
			LOG(LOG_WARN, "Could not receive data from console client: %s", strerror(bytesread));
		}

		// Return buffer to free list
		list_add(&free_buffers, &buffer->free_list);

		close(fd);
		return false;
	}

	// Increment the buffer length
	buffer->size += bytesread;

	// Start processing packet
	if (buffer->size >= sizeof(size_t))
	{
		size_t packetsize = *(size_t *)buffer->buffer;

		// Sanity check
		if (packetsize > CONSOLE_BUFFERMAX)
		{
			LOG(LOG_ERR, "Packet from client too big! Max=%d, Size=%zu", CONSOLE_BUFFERMAX, packetsize);

			// Return buffer to free list
			list_add(&free_buffers, &buffer->free_list);

			close(fd);
			return false;
		}

		if (buffer->size >= packetsize)
		{
			// We have a full packet, parse it

			void ** array = malloc0(param_arraysize("cssx"));
			if (!deserialize("cssx", array, (buffer_t)buffer->buffer))
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

						string_t rsig = string_new("%c", method_returntype(sig));

						buffer_t pbuf = aserialize(rsig.string, &ret);
						buffer_t rbuf = serialize("cssx", T_RETURN, name, rsig.string, pbuf);
						buffer_free(pbuf);

						console_replyclient(fd, rbuf);
						buffer_free(rbuf);
						syscall_free(ret);
					}
					else
					{
						string_t msg = string_new("Syscall %s with signature '%s' doesn't exist!", name, sig);

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
			FREE(array);
		}


		// Re-align new packet
		memmove(buffer->buffer, buffer->buffer + buffer->size, buffer->size - packetsize);
		buffer->size -= packetsize;
	}

	return true;

}

static bool console_newunixclient(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	int client = accept(fd, NULL, NULL);
	if (client == -1)
	{
		LOG(LOG_WARN, "Could not accept new UNIX console client socket: %s", strerror(errno));
	}
	else
	{
		// Get a free buffer
		cbuffer_t * buf = list_entry(free_buffers.next, cbuffer_t, free_list);
		list_remove(&buf->free_list);

		// Set up the buffer
		buf->size = 0;

		// Add socket to mainloop watch
		mainloop_addwatch(NULL, client, FD_READ, console_clientdata, buf);
		LOG(LOG_DEBUG, "New UNIX console client (%s)", CONSOLE_SOCKFILE);
	}

	return true;
}

static bool console_newtcpclient(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	struct sockaddr_in addr;
	socklen_t size = sizeof(addr);

	int client = accept(fd, (struct sockaddr *)&addr, &size);
	if (client == -1)
	{
		LOG(LOG_WARN, "Could not accept new TCP console client socket: %s", strerror(errno));
	}
	else
	{
		// Get a free buffer
		cbuffer_t * buf = list_entry(free_buffers.next, cbuffer_t, free_list);
		list_remove(&buf->free_list);

		// Set up the buffer
		buf->size = 0;

		// Add socket to mainloop watch
		string_t str_addr = addr2string(addr.sin_addr.s_addr);
		mainloop_addwatch(NULL, client, FD_READ, console_clientdata, buf);
		LOG(LOG_DEBUG, "New TCP console client (%s)", str_addr.string);
	}
	
	return true;
}

void module_init()
{
	exception_t * err = NULL;

	LOG(LOG_DEBUG, "Initializing console");

	// Initialize buffers
	LIST_INIT(&free_buffers);
	cbuffer_t * buffers = malloc0(sizeof(cbuffer_t) * CONSOLE_BUFFERS);

	size_t i = 0;
	for (; i<CONSOLE_BUFFERS; i++)
	{
		list_add(&free_buffers, &buffers[i].free_list);
	}

	// Start unix socket
	int unixsock = unix_server(CONSOLE_SOCKFILE, &err);
	if (err != NULL)
	{
		LOG(LOG_WARN, "Error creating unix server socket: %s", err->message.string);
		exception_free(err);
	}
	else
	{
		// Change permissions on socket (user:rw,group:rw,other:rw)
		chmod(CONSOLE_SOCKFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		mainloop_addwatch(NULL, unixsock, FD_READ, console_newunixclient, NULL);
		LOG(LOG_DEBUG, "Awaiting UNIX console clients on file %s", CONSOLE_SOCKFILE);
	}

	// Start network socket (if cfg says to)
	if (enable_network)
	{
		LOG(LOG_DEBUG, "Creating network server socket");

		int tcpsock = tcp_server(CONSOLE_TCPPORT, &err);
		if (err != NULL)
		{
			LOG(LOG_WARN, "Error creating network server socket: %s", err->message.string);
			exception_free(err);
		}
		else
		{
			mainloop_addwatch(NULL, tcpsock, FD_READ, console_newtcpclient, NULL);
			LOG(LOG_DEBUG, "Awaiting TCP console clients on port %d", CONSOLE_TCPPORT);
		}
	}
}

