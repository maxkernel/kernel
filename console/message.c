#include <unistd.h>
#include <errno.h>

#include <aul/common.h>

#include <console.h>
#include <serialize.h>
#include <message.h>


msgstate_t message_readfd(int fd, msgbuffer_t * buf)
{
	if (buf->state == P_ERROR || buf->state == P_EOF)
	{
		// Parse state check
		return buf->state;
	}

	// Reset errno
	errno = 0;

	// Read from file descriptor and put it in the buffer
	ssize_t bytesread = read(fd, &buf->buffer, CONSOLE_BUFFERMAX - buf->size);
	if (bytesread <= 0)
	{
		if (errno == EAGAIN)
		{
			// No data, but no error either
			return buf->state;
		}

		buf->state = P_EOF;
		return buf->state;
	}


	// Increment the buffer length
	buf->size += bytesread;

	switch (buf->state)
	{
		case P_FRAMING:
		{
			if (buf->size < sizeof(int))
			{
				break;
			}

			int frame = *(int *)&(buf->buffer[0]);
			if (frame != CONSOLE_FRAMEING)
			{
				buf->state = P_ERROR;
				break;
			}

			// Framing passed, move on to next step
			buf->index += sizeof(int);
			buf->state = P_HEADER;
		}

		case P_HEADER:
		{

			// Get the header data out of the packet
			void * n_buffer = &buf->buffer[buf->index];
			size_t n_size = buf->size - buf->index;

			if ((buf->msg.headersize = deserialize_2args(n_buffer, n_size, NULL, "css", &buf->msg.type, &buf->msg.name, &buf->msg.sig)) < 0)
			{
				// Couldn't parse out the header, wait until next pass
				break;
			}

			// Header passed, move on to the next step
			buf->index += buf->msg.headersize;
			buf->state = P_BODY;
		}

		case P_BODY:
		{
			// Parse the body
			void * n_buffer = &buf->buffer[buf->index];
			size_t n_size = buf->size - buf->index;

			if ((buf->msg.bodysize = deserialize_2header(buf->msg.body, sizeof(buf->msg.body), NULL, method_params(buf->msg.sig), n_buffer, n_size)) < 0)
			{
				// Couldn't parse out body, wait until next pass
				break;
			}

			// Body passed, move on to next step
			buf->index += buf->msg.bodysize;
			buf->state = P_DONE;

			break;
		}

		default:
		{
			// Should never get here
			break;
		}
	}

	return buf->state;
}

bool message_writefd(int fd, char msgtype, const char * name, const char * sig, ...)
{
	va_list args;
	va_start(args, sig);
	bool ret = message_vwritefd(fd, msgtype, name, sig, args);
	va_end(args);

	return ret;
}

bool message_vwritefd(int fd, char msgtype, const char * name, const char * sig, va_list args)
{
	char buffer[CONSOLE_BUFFERMAX];
	errno = 0;

	ssize_t hlen = serialize_2array(buffer, sizeof(buffer), NULL, "icss", CONSOLE_FRAMEING, msgtype, name, sig);
	if (hlen < 0)
	{
		return false;
	}

	ssize_t blen = vserialize_2array(buffer + hlen, sizeof(buffer) - hlen, NULL, method_params(sig), args);
	if (blen < 0)
	{
		return false;
	}

	ssize_t wrote = write(fd, buffer, hlen + blen);
	if (wrote != (hlen + blen))
	{
		return false;
	}

	return true;
}

bool message_awritefd(int fd, char msgtype, const char * name, const char * sig, void ** args)
{
	char buffer[CONSOLE_BUFFERMAX];
	errno = 0;

	ssize_t hlen = serialize_2array(buffer, sizeof(buffer), NULL, "icss", CONSOLE_FRAMEING, msgtype, name, sig);
	if (hlen < 0)
	{
		return false;
	}

	ssize_t blen = aserialize_2array(buffer + hlen, sizeof(buffer) - hlen, NULL, method_params(sig), args);
	if (blen < 0)
	{
		return false;
	}

	ssize_t wrote = write(fd, buffer, hlen + blen);
	if (wrote != (hlen + blen))
	{
		return false;
	}

	return true;
}

message_t * message_getmessage(msgbuffer_t * buf)
{
	return &buf->msg;
}

msgstate_t message_getstate(msgbuffer_t * buf)
{
	return buf->state;
}

void message_reset(msgbuffer_t * buf)
{
	// Done with the packet, remove packet from buffer
	memmove(&buf->buffer[0], &buf->buffer[buf->index], buf->size - buf->index);
	buf->size -= buf->index;
	buf->index = 0;

	// Reset for next pass
	memset(&buf->msg, 0, sizeof(message_t));
	buf->state = P_FRAMING;
}

void message_clear(msgbuffer_t * buf)
{
	memset(buf, 0, sizeof(msgbuffer_t));
}
