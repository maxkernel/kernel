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


bool enable_network = false;

static list_t free_buffers;

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
						LOG(LOG_WARN, "Could not execute syscall %s with sig %s: Code %d %s", msg->name, msg->sig, e->code, e->message);
						string_t payload = string_new("Syscall %s with signature '%s' failed: Code %d %s", msg->name, msg->sig, e->code, e->message);
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
	if (client == -1)
	{
		LOG(LOG_WARN, "Could not accept new console client socket: %s", strerror(errno));
	}
	else
	{
		if (list_isempty(&free_buffers))
		{
			LOG(LOG_INFO, "Console out of free buffers! Consider increasing CONSOLE_BUFFERS in console module (currently %d)", CONSOLE_BUFFERS);
			close(client);
			return true;
		}

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

bool module_init()
{
	// Initialize buffers
	LIST_INIT(&free_buffers);
	msgbuffer_t * buffers = malloc0(sizeof(msgbuffer_t) * CONSOLE_BUFFERS);

	size_t i = 0;
	for (; i<CONSOLE_BUFFERS; i++)
	{
		list_add(&free_buffers, &buffers[i].free_list);
	}

	// Start unix socket
	{
		exception_t * e = NULL;
		int unixsock = unix_server(CONSOLE_SOCKFILE, &e);

		if (unixsock == -1 || exception_check(&e))
		{
			LOG(LOG_ERR, "Error creating unix server socket: %s", (e == NULL)? "Unknown error" : e->message);
			exception_free(e);
		}
		else
		{
			// Change permissions on socket (user:rw,group:rw,other:rw)
			chmod(CONSOLE_SOCKFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
			mainloop_addwatch(NULL, unixsock, FD_READ, console_newclient, NULL);
			LOG(LOG_DEBUG, "Awaiting UNIX console clients on file %s", CONSOLE_SOCKFILE);
		}
	}

	// Start network socket (if config'd)
	{
		exception_t * e = NULL;

		if (enable_network)
		{
			LOG(LOG_DEBUG, "Creating network server socket");

			int tcpsock = tcp_server(CONSOLE_TCPPORT, &e);
			if (tcpsock == -1 || exception_check(&e))
			{
				LOG(LOG_WARN, "Error creating network server socket: %s", (e == NULL)? "Unknown error" : e->message);
				exception_free(e);
			}
			else
			{
				mainloop_addwatch(NULL, tcpsock, FD_READ, console_newclient, NULL);
				LOG(LOG_DEBUG, "Awaiting TCP console clients on port %d", CONSOLE_TCPPORT);
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

config_param(enable_network, 'b', "Allow syscalls to be executed over the network (TCP)");
