#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <poll.h>

#include <aul/net.h>

#include <console.h>
#include <message.h>
#include <serialize.h>
#include <max.h>


void max_initialize(maxhandle_t * hand)
{
	memset(hand, 0, sizeof(maxhandle_t));

	hand->malloc = malloc;
	hand->free = free;
	hand->memerr = max_memerr;

	hand->syscall_cache = NULL;

	hand->sock = -1;
	mutex_init(&hand->sock_mutex, M_RECURSIVE);

	hand->timeout = DEFAULT_TIMEOUT;
}

void * max_destroy(maxhandle_t * hand)
{
	max_syscallcache_destroy(hand);

	if (hand->sock != -1)
	{
		close(hand->sock);
		hand->sock = -1;
	}

	return hand->userdata;
}

void max_setmalloc(maxhandle_t * hand, malloc_f mfunc, free_f ffunc, memerr_f efunc)
{
	if (mfunc != NULL) hand->malloc = mfunc;
	if (ffunc != NULL) hand->free = ffunc;
	if (efunc != NULL) hand->memerr = efunc;
}

void max_memerr()
{
	fprintf(stderr, "Error: Out of memory\n");
	fflush(NULL);
	exit(EXIT_FAILURE);
}



bool max_connectlocal(maxhandle_t * hand, exception_t ** err)
{
	return max_connect(hand, HOST_UNIXSOCK CONSOLE_SOCKFILE, err);
}

bool max_connect(maxhandle_t * hand, const char * host, exception_t ** err)
{
	if (exception_check(err))
	{
		// Exception already set
		return false;
	}

	if (hand->sock != -1)
	{
		// Socket already open
		exception_set(err, EALREADY, "Socket already open!");
		return false;
	}

	if (strcmp(host, HOST_LOCAL) == 0)
	{
		// Cheap hack to recurse back to this function with correct unixsock file
		return max_connectlocal(hand, err);
	}
	else if (strprefix(host, HOST_UNIXSOCK))
	{
		// Connect over unix socket
		const char * file = &host[strlen(HOST_UNIXSOCK)];
		int sock = unix_client(file, err);

		if (exception_check(err))
		{
			// Something bad happened while setting up the unix socket
			return false;
		}

		hand->sock = sock;
	}
	else if (strprefix(host, HOST_IP))
	{
		// Connect directly to IP
		exception_set(err, ENOSYS, "Unsupported protocol (future release)");
		return false;
	}
	else if (strprefix(host, HOST_UID))
	{
		// Connect to the kernel with the given UID
		exception_set(err, ENOSYS, "Unsupported protocol (future release)");
		return false;
	}
	else
	{
		exception_set(err, EINVAL, "Unknown protocol/host: %s", host);
		return false;
	}

	return true;
}

bool max_syscall(maxhandle_t * hand, exception_t ** err, const char * syscall, const char * sig, return_t * ret, ...)
{
	va_list args;
	va_start(args, ret);
	bool r = max_vsyscall(hand, err, syscall, sig, ret, args);
	va_end(args);

	return r;
}

static bool max_waitreply(maxhandle_t * hand, exception_t ** err, return_t * ret, const char * syscall)
{
	struct pollfd pfd;
	pfd.fd = hand->sock;
	pfd.events = POLLIN | POLLERR;

	msgbuffer_t msgbuf;
	message_clear(&msgbuf);

	errno = 0;
	while (message_getstate(&msgbuf) < P_DONE)
	{
		// TODO - we should probably wait a *maximum* of hand->timeout throughout *all* poll's, not just for each poll

		int status = poll(&pfd, 1, hand->timeout);
		if (status < 0)
		{
			exception_set(err, errno, "Could not poll for response to syscall %s: %s", syscall, strerror(errno));
			return false;
		}
		else if (status == 0)
		{
			exception_set(err, ETIMEDOUT, "Timed out waiting for response to syscall %s", syscall);
			return false;
		}

		message_readfd(hand->sock, &msgbuf);
	}

	if (message_getstate(&msgbuf) != P_DONE)
	{
		exception_set(err, errno, "Error while reading syscall %s response", syscall);
		return false;
	}

	message_t * msg = message_getmessage(&msgbuf);

	// TODO - make sure that this response is actually the response to the syscall we just sent

	return_t r = {
		.handle = hand,
		.type = msg->type,
		.sig = method_returntype(msg->sig),
	};

	void copy(const void * ptr, size_t s)
	{
		memcpy(&r.data, ptr, s);
	}

	switch (r.sig)
	{
		case T_VOID:												break;
		case T_BOOLEAN:		copy(msg->body[0], sizeof(bool));		break;
		case T_INTEGER:		copy(msg->body[0], sizeof(int));		break;
		case T_DOUBLE:		copy(msg->body[0], sizeof(double));		break;
		case T_CHAR:		copy(msg->body[0], sizeof(char));		break;

		case T_STRING:
		{
			char ** strp = msg->body[0];
			copy(*strp, strlen(*strp)+1);
			break;
		}

		default:
		{
			exception_set(err, EINVAL, "Unknown return type returned from syscall %s: %c", syscall, r.sig);
			return false;
		}
	}


	if (ret != NULL)
	{
		memcpy(ret, &r, sizeof(return_t));
	}

	return true;
}

bool max_vsyscall(maxhandle_t * hand, exception_t ** err, const char * syscall, const char * sig, return_t * ret, va_list args)
{
	if (exception_check(err))
	{
		// Error already set
		return false;
	}

	bool r = false;
	mutex_lock(&hand->sock_mutex);
	{
		if (!message_vwritefd(hand->sock, T_METHOD, syscall, sig, args))
		{
			exception_set(err, errno, "Could not send syscall %s rpc: %s", syscall, strerror(errno));
			r = false;
		}
		else
		{
			r = max_waitreply(hand, err, ret, syscall);
		}
	}
	mutex_unlock(&hand->sock_mutex);

	return r;
}

bool max_asyscall(maxhandle_t * hand, exception_t ** err, const char * syscall, const char * sig, return_t * ret, void ** args)
{
	if (exception_check(err))
	{
		// Error already set
		return false;
	}

	bool r = false;
	mutex_lock(&hand->sock_mutex);
	{
		if (!message_awritefd(hand->sock, T_METHOD, syscall, sig, args))
		{
			exception_set(err, errno, "Could not send syscall %s rpc: %s", syscall, strerror(errno));
			r = false;
		}
		else
		{
			r = max_waitreply(hand, err, ret, syscall);
		}
	}
	mutex_unlock(&hand->sock_mutex);

	return r;
}

void max_settimeout(maxhandle_t * hand, int newtimeout)
{
	hand->timeout = newtimeout;
}

int max_gettimeout(maxhandle_t * hand)
{
	return hand->timeout;
}

void max_setuserdata(maxhandle_t * hand, void * userdata)
{
	hand->userdata = userdata;
}

void * max_getuserdata(maxhandle_t * hand)
{
	return hand->userdata;
}

size_t max_getbuffersize(maxhandle_t * hand)
{
	return CONSOLE_BUFFERMAX;
}

size_t max_getheadersize(maxhandle_t * hand)
{
	return CONSOLE_HEADERSIZE;
}
