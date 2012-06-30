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


typedef struct
{
	list_t free_list;
	fdwatcher_t socket;
	msgbuffer_t buffer;
} client_t;

// TODO - add mutex around free_clients!
static stack_t free_clients;
static bool enable_network = false;

static fdwatcher_t unix_watcher;
static fdwatcher_t tcp_watcher;

static bool console_newdata(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	client_t * client = userdata;
	msgbuffer_t * buffer = &client->buffer;
	msgstate_t state = message_readfd(fd, buffer);

	if (state >= P_ERROR)
	{
		// Add buffer back to free list
		stack_push(&free_clients, &client->free_list);

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
	int sock = accept(fd, NULL, NULL);
	if (sock < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			LOG(LOG_WARN, "Could not accept new console client socket: %s", strerror(errno));
		}

		return true;
	}

	LOG(LOG_DEBUG, "New console client.");

	// TODO IMPORTANT - make non-block

	// Get a free buffer
	list_t * client_entry = stack_pop(&free_clients);
	if (client_entry == NULL)
	{
		LOG(LOG_WARN, "Console out of free buffers! (CONSOLE_BUFFERS = %d)", CONSOLE_MAXCLIENTS);
		close(sock);
		return true;
	}

	client_t * client = list_entry(client_entry, client_t, free_list);

	// Clear the message
	message_clear(&client->buffer);

	// Create the socket watcher
	watcher_newfd(&client->socket, sock, FD_READ, console_newdata, client);

	// Add socket to mainloop watch
	exception_t * e = NULL;
	if (!mainloop_addwatcher(loop, &client->socket, &e) || exception_check(&e))
	{
		LOG(LOG_ERR, "Could not add console client to mainloop: %s", exception_message(e));
		exception_free(e);

		// Destroy the watcher
		watcher_close(&client->socket);

		// Add buffer back to free list
		stack_push(&free_clients, &client->free_list);
		return true;
	}

	return true;
}

static bool console_init()
{
	labels(after_unix, after_network);

	// Initialize buffers
	{
		client_t * clients = malloc(sizeof(client_t) * CONSOLE_MAXCLIENTS);
		memset(clients, 0, sizeof(client_t) * CONSOLE_MAXCLIENTS);

		stack_init(&free_clients);
		for (size_t i = 0; i < CONSOLE_MAXCLIENTS; i++)
		{
			watcher_init(&clients[i].socket);
			stack_push(&free_clients, &clients[i].free_list);
		}
	}

	// Initialize the watchers
	{
		watcher_init(&unix_watcher);
		watcher_init(&tcp_watcher);
	}


	// Start unix socket
	{
		exception_t * e = NULL;

		int unixsock = unix_server(CONSOLE_SOCKFILE, &e);
		if (unixsock == -1 || exception_check(&e))
		{
			LOG(LOG_ERR, "Could not creat unix console socket: %s", exception_message(e));
			exception_free(e);
			goto after_unix;
		}

		// Change permissions on socket (user:rw,group:rw,other:rw)
		if (chmod(CONSOLE_SOCKFILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0)
		{
			LOG(LOG_ERR, "Could not set permissions on unix console socket: %s", strerror(errno));
			close(unixsock);
			goto after_unix;
		}

		watcher_newfd(&unix_watcher, unixsock, FD_READ, console_newclient, NULL);
		if (!mainloop_addwatcher(kernel_mainloop(), &unix_watcher, &e) || exception_check(&e))
		{
			LOG(LOG_ERR, "Could not add console unix_socket fd to mainloop: %s", exception_message(e));
			exception_free(e);
			// TODO - free watcher
			goto after_unix;
		}

		LOG(LOG_DEBUG, "Awaiting console clients on file %s", CONSOLE_SOCKFILE);
	}
after_unix:

	// Start network socket (if config'd)
	if (enable_network)
	{
		exception_t * e = NULL;
		LOG(LOG_DEBUG, "Enabling console over network");

		int tcpsock = tcp_server(CONSOLE_TCPPORT, &e);
		if (tcpsock == -1 || exception_check(&e))
		{
			LOG(LOG_WARN, "Error creating network server socket: %s", exception_message(e));
			exception_free(e);
			goto after_network;
		}

		watcher_newfd(&tcp_watcher, tcpsock, FD_READ, console_newclient, NULL);
		if (!mainloop_addwatcher(kernel_mainloop(), &tcp_watcher, &e) || exception_check(&e))
		{
			LOG(LOG_ERR, "Could not add console tcp fd to mainloop: %s", exception_message(e));
			exception_free(e);
			// TODO - free watcher
			goto after_network;
		}

		LOG(LOG_DEBUG, "Awaiting console clients on tcp port %d", CONSOLE_TCPPORT);
	}
after_network:

	return true;
}


module_name("Console");
module_version(1,0,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Provides syscall RPC from other processes via unix/tcp socket");
module_oninitialize(console_init);

module_config(enable_network, 'b', "Allow syscalls to be executed over the network (TCP)");
