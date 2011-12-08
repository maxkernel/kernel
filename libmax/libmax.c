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


typedef struct
{
	const char * name;
	const char * sig;
	const char * description;

	hashentry_t syscalls_entry;
} syscall_t;

#if 0
#define bad_malloc(handle, ptr) __bad_malloc(handle, ptr, __FILE__, __LINE__)
static inline bool __bad_malloc(maxhandle_t * hand, void * ptr, const char * file, unsigned int line)
{
	if (ptr == NULL)
	{
		exception_make(hand->error, ENOMEM, "Failed to allocate memory (%s:%d)", file, line);
		hand->memerr();
		return true;
	}

	return false;
}
#endif


void max_init(maxhandle_t * hand)
{
	PZERO(hand, sizeof(maxhandle_t));

	hand->malloc = malloc;
	hand->free = free;
	hand->memerr = max_memerr;

	HASHTABLE_INIT(&hand->syscalls, hash_str, hash_streq);

	hand->sock = -1;
	mutex_init(&hand->sock_mutex, M_RECURSIVE);

	hand->timeout = DEFAULT_TIMEOUT;
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
	return max_connect(hand, err, HOST_UNIXSOCK CONSOLE_SOCKFILE);
}

bool max_connect(maxhandle_t * hand, exception_t ** err, const char * host)
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

	if (strprefix(host, HOST_UNIXSOCK))
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
		exception_set(err, ENOSYS, "Unsupported (future release)");
		return false;
	}
	else if (strprefix(host, HOST_UID))
	{
		// Connect to the kernel with the given UID
		exception_set(err, ENOSYS, "Unsupported (future release)");
		return false;
	}
	else
	{
		exception_set(err, EINVAL, "Unknown protocol/host: %s", host);
		return false;
	}

	return true;
}

void max_close(maxhandle_t * hand)
{
	close(hand->sock);
}

bool max_syscall(maxhandle_t * hand, exception_t ** err, return_t * ret, const char * syscall, const char * sig, ...)
{
	va_list args;
	va_start(args, sig);
	bool r = max_vsyscall(hand, err, ret, syscall, sig, args);
	va_end(args);

	return r;
}

bool max_vsyscall(maxhandle_t * hand, exception_t ** err, return_t * ret, const char * syscall, const char * sig, va_list args)
{
	if (exception_check(err))
	{
		// Error already set
		return false;
	}

	if (!message_vwritefd(hand->sock, T_METHOD, syscall, sig, args))
	{
		exception_set(err, errno, "Could not send syscall %s rpc: %s", syscall, strerror(errno));
		return false;
	}

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

	return_t r = {
		.handle = hand,
		.type = method_returntype(msg->sig),
	};

	void copy(const void * ptr, size_t s)
	{
		memcpy(&r.data, ptr, s);
	}

	switch (r.type)
	{
		case T_VOID:
		{
			break;
		}

		case T_BOOLEAN:
		{
			copy(msg->body[0], sizeof(bool));
			break;
		}

		case T_INTEGER:
		{
			copy(msg->body[0], sizeof(int));
			break;
		}

		case T_DOUBLE:
		{
			copy(msg->body[0], sizeof(double));
			break;
		}

		case T_CHAR:
		{
			copy(msg->body[0], sizeof(char));
			break;
		}

		case T_STRING:
		{
			char ** strp = msg->body[0];
			copy(*strp, strlen(*strp)+1);
			break;
		}

		default:
		{
			exception_set(err, EINVAL, "Unknown return type returned from syscall %s: %c", syscall, r.type);
			return false;
		}
	}


	if (ret != NULL)
	{
		memcpy(ret, &r, sizeof(return_t));
	}

	return true;
}

#if 0
return_t max_asyscall(maxhandle_t * hand, const char * syscall, const char * sig, void ** args)
{
	char buffer[CONSOLE_BUFFERMAX];

	errno = 0;

	ssize_t hlength = serialize_2array(buffer, sizeof(buffer), "icss", CONSOLE_FRAMEING, T_METHOD, syscall, sig);
	if (hlength == -1)
	{
		return make_error(hand, errno, "Could not serialize packet header");
	}

	ssize_t blength = aserialize_2array(buffer+hlength, sizeof(buffer) - hlength, method_params(sig), args);
	if (blength == -1)
	{
		return make_error(hand, errno, "Could not serialize packet parameters");
	}

	ssize_t wrote = write(hand->sock, buffer, hlength + blength);
	if (wrote != (hlength + blength))
	{
		return make_error(hand, (errno != 0)? errno : EIO, "Could not write all data");
	}

	struct pollfd pfd;
	pfd.fd = hand->sock;
	pfd.events = POLLIN | POLLERR;

	int status = poll(&pfd, 1, hand->timeout);
	if (status < 0)
	{
		return make_error(hand, errno, "Could not poll for response to syscall");
	}
	else if (status == 0)
	{
		return make_error(hand, ETIMEDOUT, "Timed out waiting for response");
	}

	if (pfd.revents & POLLIN)
	{
		// Parse out response

		//TODO - finish this function!
		return_t ret;
		ret.handle = hand;
		ret.type = T_VOID;

		return ret;
	}
	else if (pfd.revents & POLLERR)
	{
		// Error happened on the file descriptor
		return make_error(hand, EIO, "Error while polling file descriptor");
	}

	return make_error(hand, EIO, "Should not have gotten here!");
}
#endif

void max_settimeout(maxhandle_t * hand, int newtimeout)
{
	hand->timeout = newtimeout;
}

int max_gettimeout(maxhandle_t * hand)
{
	return hand->timeout;
}
