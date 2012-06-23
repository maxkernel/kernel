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
#include <aul/stack.h>

#include <kernel.h>
#include <serialize.h>
#include <console.h>
#include <message.h>


bool enable_network = false;

static stack_t free_buffers;

static bool console_clientdata(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	msgbuffer_t * buffer = userdata;
	msgstate_t state = message_readfd(fd, buffer);

	if (state >= P_ERROR)
	{
		// Add buffer back to free list
		stack_push(&free_buffers, &buffer->free_list);

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
					exception_t * e = NULL;
					sysreturn_t r = {0};

					bool success = asyscall_exec(msg->name, &e, &r, msg->body);
					if (success)
					{
						// Pack and send a return message
						char rsig[] = { method_returntype(msg->sig), '\0' };
						void * rpack = &r;
						message_awritefd(fd, T_RETURN, msg->name, rsig, &rpack);
					}
					else
					{
						// Some error happened!
						LOG(LOG_WARN, "Could not execute syscall %s with sig %s: %s", msg->name, msg->sig, exception_message(e));
						string_t payload = string_new("Syscall %s with signature '%s' failed: %s", msg->name, msg->sig, exception_message(e));
						message_writefd(fd, T_ERROR, msg->name, "s", payload.string);
					}

					// Free exception if set
					exception_free(e);
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
}

static bool console_newclient(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	int client = accept(fd, NULL, NULL);
	if (client < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			LOG(LOG_WARN, "Could not accept new console client socket: %s", strerror(errno));
		}

		return true;
	}

	LOG(LOG_DEBUG, "New console client.");

	// Get a free buffer
	list_t * entry = stack_pop(&free_buffers);
	if (entry == NULL)
	{
		LOG(LOG_WARN, "Console out of free buffers! Consider increasing CONSOLE_BUFFERS in console module (currently %d)", CONSOLE_BUFFERS);
		close(client);
		return true;
	}

	msgbuffer_t * buffer = list_entry(entry, msgbuffer_t, free_list);

	// Clear the message
	message_clear(buffer);

	// Add socket to mainloop watch
	exception_t * e = NULL;
	if (!mainloop_addfdwatch(NULL, client, FD_READ, console_clientdata, buffer, &e))
	{
		LOG(LOG_ERR, "Could not add console client to mainloop: %s", exception_message(e));

		// Add buffer back to free list
		stack_push(&free_buffers, &buffer->free_list);

		return true;
	}

	return true;
}

bool module_init()
{
	// Initialize buffers
	stack_init(&free_buffers);
	msgbuffer_t * buffers = malloc(sizeof(msgbuffer_t) * CONSOLE_BUFFERS);
	memset(buffers, 0, sizeof(msgbuffer_t) * CONSOLE_BUFFERS);

	for (size_t i = 0; i<CONSOLE_BUFFERS; i++)
	{
		stack_push(&free_buffers, &buffers[i].free_list);
	}

	// Start unix socket
	{
		exception_t * e = NULL;
		int unixsock = unix_server(CONSOLE_SOCKFILE, &e);

		if (unixsock == -1 || exception_check(&e))
		{
			LOG(LOG_ERR, "Error creating unix server socket: %s", exception_message(e));
			exception_free(e);
		}
		else
		{
			// Change permissions on socket (user:rw,group:rw,other:rw)
			chmod(CONSOLE_SOCKFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

			exception_t * e = NULL;
			if (!mainloop_addfdwatch(NULL, unixsock, FD_READ, console_newclient, NULL, &e))
			{
				LOG(LOG_ERR, "Could not add console unix_socket fd to mainloop: %s", exception_message(e));
				exception_free(e);
			}
			else
			{
				LOG(LOG_DEBUG, "Awaiting console clients on file %s", CONSOLE_SOCKFILE);
			}
		}
	}

	// Start network socket (if config'd)
	if (enable_network)
	{
		LOG(LOG_DEBUG, "Creating network server socket");

		exception_t * e = NULL;
		int tcpsock = tcp_server(CONSOLE_TCPPORT, &e);
		if (tcpsock == -1 || exception_check(&e))
		{
			LOG(LOG_WARN, "Error creating network server socket: %s", exception_message(e));
			exception_free(e);
		}
		else
		{
			exception_t * e = NULL;
			if (!mainloop_addfdwatch(NULL, tcpsock, FD_READ, console_newclient, NULL, &e))
			{
				LOG(LOG_ERR, "Could not add console tcp fd to mainloop: %s", exception_message(e));
				exception_free(e);
			}
			else
			{
				LOG(LOG_DEBUG, "Awaiting console clients on tcp port %d", CONSOLE_TCPPORT);
			}
		}
	}

	return true;
}


module_name("Console");
module_version(1,0,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Provides syscall RPC from other processes via unix/tcp socket");
module_oninitialize(module_init);

module_config(enable_network, 'b', "Allow syscalls to be executed over the network (TCP)");
