#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <netinet/in.h>

#include <aul/common.h>
#include <aul/net.h>
#include <aul/mainloop.h>
#include <aul/list.h>

#include <kernel.h>
#include <serialize.h>
#include <console.h>
#include <message.h>

MOD_INIT(module_init);

CFG_PARAM(enable_network, "b");
bool enable_network = false;


static list_t free_buffers;


#if 0
static string_t addr2string(uint32_t ip)
{
	unsigned char a4 = (ip & 0xFF000000) >> 24;
	unsigned char a3 = (ip & 0x00FF0000) >> 16;
	unsigned char a2 = (ip & 0x0000FF00) >> 8;
	unsigned char a1 = ip & 0xFF;
	return string_new("%d.%d.%d.%d", a1, a2, a3, a4);
}
#endif

static bool console_clientdata(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	msgbuffer_t * buffer = userdata;
	msgstate_t state = message_readfd(fd, buffer);

	if (state >= P_ERROR)
	{
		// Add buffer back to free list
		list_add(&free_buffers, &buffer->free_list);

		return false;
	}

	if (state == P_DONE)
	{
		message_t * msg = message_getmessage(buffer);
		switch (msg->type)
		{
			case T_METHOD:
			{
				if (syscall_exists(msg->name, msg->sig))
				{
					// Execute the syscall
					void * r = asyscall_exec(msg->name, msg->body);

					// Pack and send a return message
					char rsig[] = { method_returntype(msg->sig), '\0' };

					switch (method_returntype(msg->sig))
					{
						case T_VOID:
						{
							message_writefd(fd, T_RETURN, msg->name, rsig);
							break;
						}

						case T_BOOLEAN:
						{
							message_writefd(fd, T_RETURN, msg->name, rsig, *(bool *)r);
							break;
						}

						case T_INTEGER:
						{
							message_writefd(fd, T_RETURN, msg->name, rsig, *(int *)r);
							break;
						}

						case T_DOUBLE:
						{
							message_writefd(fd, T_RETURN, msg->name, rsig, *(double *)r);
							break;
						}

						case T_CHAR:
						{
							message_writefd(fd, T_RETURN, msg->name, rsig, *(char *)r);
							break;
						}

						case T_STRING:
						{
							message_writefd(fd, T_RETURN, msg->name, rsig, *(char **)r);
							break;
						}
					}

					// Free up resources
					syscall_free(r);
				}
				else
				{
					LOG(LOG_WARN, "Could not execute syscall %s with sig %s. Syscall doesn't exist!", msg->name, msg->sig);

					// Syscall doesn't exist, return error
					string_t payload = string_new("Syscall %s with signature '%s' doesn't exist!", msg->name, msg->sig);
					message_writefd(fd, T_ERROR, msg->name, "s", payload.string);
				}

				break;
			}

			case T_ERROR:
			{
				// Client sent an error, just log it
				LOG(LOG_WARN, "Console received an error from client: (%s) %s", msg->name, (strcmp(msg->sig, "s") == 0)? *(char **)msg->body[0] : "Unknown error");
				break;
			}

			case T_RETURN:
			{
				LOG(LOG_WARN, "Unexpected return type message received in console (name=%s)", msg->name);
				break;
			}

			default:
			{
				LOG(LOG_WARN, "Unknown message type received in console (name=%s, type=%c)", msg->name, msg->type);
				break;
			}
		}

		message_reset(buffer);
	}

	return true;


#if 0
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

			void * array[CONSOLE_HEADERSIZE];

			if (deserialize_2header(array, sizeof(array), "cssx", buffer->buffer + sizeof(size_t), buffer->size - sizeof(size_t)) == -1)
			{
				LOG(LOG_WARN, "Could not deserialize syscall buffer: %s", strerror(errno));
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

		}

err_badparse:
		// Re-align new packet
		memmove(buffer->buffer, buffer->buffer + buffer->size, buffer->size - packetsize);
		buffer->size -= packetsize;
	}

	return true;
#endif

}

static bool console_newclient(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	int client = accept(fd, NULL, NULL);
	if (client == -1)
	{
		LOG(LOG_WARN, "Could not accept new console client socket: %s", strerror(errno));
	}
	else
	{
		// Get a free buffer
		msgbuffer_t * msg = list_entry(free_buffers.next, msgbuffer_t, free_list);
		list_remove(&msg->free_list);

		// Clear the message
		message_clear(msg);

		// Add socket to mainloop watch
		mainloop_addwatch(NULL, client, FD_READ, console_clientdata, msg);
		LOG(LOG_DEBUG, "New console client.");
	}

	return true;
}

void module_init()
{
	exception_t * err = NULL;

	LOG(LOG_DEBUG, "Initializing console");

	// Initialize buffers
	LIST_INIT(&free_buffers);
	msgbuffer_t * buffers = malloc0(sizeof(msgbuffer_t) * CONSOLE_BUFFERS);

	size_t i = 0;
	for (; i<CONSOLE_BUFFERS; i++)
	{
		list_add(&free_buffers, &buffers[i].free_list);
	}

	// Start unix socket
	int unixsock = unix_server(CONSOLE_SOCKFILE, &err);
	if (err != NULL)
	{
		LOG(LOG_WARN, "Error creating unix server socket: %s", err->message);
		exception_free(err);
	}
	else
	{
		// Change permissions on socket (user:rw,group:rw,other:rw)
		chmod(CONSOLE_SOCKFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		mainloop_addwatch(NULL, unixsock, FD_READ, console_newclient, NULL);
		LOG(LOG_DEBUG, "Awaiting UNIX console clients on file %s", CONSOLE_SOCKFILE);
	}

	// Start network socket (if cfg says to)
	if (enable_network)
	{
		LOG(LOG_DEBUG, "Creating network server socket");

		int tcpsock = tcp_server(CONSOLE_TCPPORT, &err);
		if (err != NULL)
		{
			LOG(LOG_WARN, "Error creating network server socket: %s", err->message);
			exception_free(err);
		}
		else
		{
			mainloop_addwatch(NULL, tcpsock, FD_READ, console_newclient, NULL);
			LOG(LOG_DEBUG, "Awaiting TCP console clients on port %d", CONSOLE_TCPPORT);
		}
	}
}

