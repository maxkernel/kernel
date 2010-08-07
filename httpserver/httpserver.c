#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <regex.h>

#include "httpserver.h"
#include <aul/net.h>
#include <aul/contrib/list.h>
#include <kernel.h>

MOD_PREACT(module_preact);

#define NUM_BUFFERS		10
#define BUFFER_LEN		500

static regex_t get_match;
static regex_t params_match;
static regex_t header_match;

typedef struct
{
	const char * uri;
	http_match match;
	http_callback callback;
	void * userdata;

	struct list_head filter;
} http_filter;

typedef struct
{
	bool inuse;
	char buffer[BUFFER_LEN];
	size_t length;

	http_context * ctx;
} http_buffer;

struct __http_context
{
	int socket;
	struct list_head filters;
	http_buffer buffers[NUM_BUFFERS];

	mainloop_t * mainloop;
	hashtable_t headers;
	hashtable_t parameters;
};

static inline String addr2string(uint32_t ip)
{
	unsigned char a4 = (ip & 0xFF000000) >> 24;
	unsigned char a3 = (ip & 0x00FF0000) >> 16;
	unsigned char a2 = (ip & 0x0000FF00) >> 8;
	unsigned char a1 = ip & 0xFF;
	return string_new("%d.%d.%d.%d", a1, a2, a3, a4);
}

static bool http_newdata(int fd, fdcond_t cond, void * userdata)
{
	http_buffer * buffer = userdata;

	ssize_t bytesread = recv(fd, buffer->buffer + buffer->length, BUFFER_LEN - buffer->length - 1, 0);
	if (bytesread > 0)
	{
		buffer->length += bytesread;
		buffer->buffer[buffer->length] = '\0';

		if (strstr(buffer->buffer, "\r\n\r\n") != NULL)
		{
			// We have the full header, parse it
			regmatch_t match[3];
			ZERO(match);

			if (regexec(&get_match, buffer->buffer, 3, match, 0) == 0)
			{
				// The browser is using GET. Gooood.
				char request_uri[250] = {0};
				memcpy(request_uri, buffer->buffer + match[1].rm_so, MIN(sizeof(request_uri) - 1, match[1].rm_eo - match[1].rm_so));

				// Parse the GET parameters off the URI
				hashtable_clear(&buffer->ctx->parameters);
				if (strchr(request_uri, '?') != NULL)
				{
					char * params = strchr(request_uri, '?');
					*params++ = '\0';

					size_t offset = 0;
					while (regexec(&params_match, params + offset, 3, match, 0) == 0)
					{
						bool atend = params[offset + match[0].rm_eo] == '\0';

						char * key = params + offset + match[1].rm_so;
						key[match[1].rm_eo - match[1].rm_so] = '\0';

						char * value = params + offset + match[2].rm_so;
						value[match[2].rm_eo - match[2].rm_so] = '\0';

						hashtable_put(&buffer->ctx->parameters, key, value);

						if (atend)
							break;
						else
							offset += match[0].rm_eo+1;
					}
				}

				// Pass the request through the filters
				http_filter * pass_match = NULL;
				struct list_head * pos;
				list_for_each(pos, &buffer->ctx->filters)
				{
					http_filter * filter = list_entry(pos, http_filter, filter);
					switch (filter->match)
					{
						case MATCH_ALL:
						{
							if (strcmp(request_uri, filter->uri) == 0)
								pass_match = filter;

							break;
						}

						case MATCH_PREFIX:
						{
							if (strprefix(request_uri, filter->uri))
								pass_match = filter;

							break;
						}
					}

					if (pass_match != NULL)
					{
						break;
					}
				}

				if (pass_match != NULL)
				{
					// We have matched (at least) one filter. Parse the rest of the headers
					hashtable_t * headers = &buffer->ctx->headers;
					hashtable_clear(headers);

					size_t offset = 0;
					while (regexec(&header_match, buffer->buffer + offset, 3, match, 0) == 0)
					{
						char * key = buffer->buffer + offset + match[1].rm_so;
						key[match[1].rm_eo - match[1].rm_so] = '\0';

						char * value = buffer->buffer + offset + match[2].rm_so;
						value[match[2].rm_eo - match[2].rm_so] = '\0';

						hashtable_put(headers, key, value);

						offset += match[0].rm_eo+1;
					}

					pass_match->callback(&fd, headers, &buffer->ctx->parameters, request_uri);
				}
				else
				{
					// We have not passed the filters. Output 404
					const char * msg = "HTTP/1.0 404 Not Found\r\n\r\n";
					send(fd, msg, strlen(msg)+1, 0);
				}
			}

		}
		else if (buffer->length == BUFFER_LEN-1)
		{
			// Buffer overrun. We couldn't read all the header data. Too bad
			LOG(LOG_WARN, "Buffer overrun while reading headers in http_newdata. Consider increasing the buffer length (currently %d)", BUFFER_LEN);
		}
		else
		{
			// We only have some of the header... no matter, just return true and we'll get the rest on the next pass
			return true;
		}
	}

	buffer->inuse = false;
	close(fd);
	return false;
}

static bool http_newclient(int fd, fdcond_t cond, void * userdata)
{
	http_context * ctx = userdata;

	struct sockaddr_in addr;
	socklen_t socklen = sizeof(addr);
	int sock = accept(fd, (struct sockaddr *)&addr, &socklen);
	if (sock == -1)
	{
		LOG(LOG_WARN, "Could not accept new http client socket: %s", strerror(errno));
	}
	else
	{
		http_buffer * buffer = NULL;
		int i;

		for (i=0; i<NUM_BUFFERS; i++)
		{
			if (!ctx->buffers[i].inuse)
				buffer = &ctx->buffers[i];
		}

		if (buffer == NULL)
		{
			LOG(LOG_WARN, "Could not accept new HTTP client: Out of HTTP buffers!");
			close(sock);
		}
		else
		{
			// Set up the buffer
			buffer->inuse = true;
			buffer->buffer[0] = '\0';
			buffer->length = 0;
			buffer->ctx = ctx;

			// Now set up the watch on the accept'd file descriptor
			mainloop_addwatch(ctx->mainloop, sock, FD_READ, http_newdata, buffer);
		}
	}


	return true;
}

http_context * http_new(uint16_t port, mainloop_t * mainloop, Error ** error)
{
	if (error_check(error))
	{
		LOG(LOG_ERR, "Error already set in function http_new");
		return NULL;
	}

	http_context * ctx = malloc0(sizeof(http_context));
	ctx->mainloop = mainloop;
	ctx->headers = hashtable_new(hash_str, hash_streq);
	ctx->parameters = hashtable_new(hash_str, hash_streq);
	INIT_LIST_HEAD(&ctx->filters);

	Error * err = NULL;

	ctx->socket = tcp_server(port, &err);
	if (err != NULL)
	{
		if (error != NULL)
			*error = err;
		else
			error_free(err);

		free(ctx);
		return NULL;
	}

	mainloop_addwatch(mainloop, ctx->socket, FD_READ, http_newclient, ctx);
	return ctx;
}

void http_adduri(http_context * ctx, const char * uri, http_match match, http_callback cb, void * userdata)
{
	http_filter * filt = malloc0(sizeof(http_filter));
	filt->uri = uri;
	filt->match = match;
	filt->callback = cb;
	filt->userdata = userdata;

	list_add_tail(&filt->filter, &ctx->filters);
}

void http_printf(http_connection * conn, const char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	http_vprintf(conn, fmt, args);
	va_end(args);
}

void http_vprintf(http_connection * conn, const char * fmt, va_list args)
{
	String msg = string_blank();
	string_vappend(&msg, fmt, args);
	http_write(conn, msg.string, msg.length);
}

void http_write(http_connection * conn, const void * buf, size_t len)
{
	send(*conn, buf, len, 0);
}

void http_destroy(http_context * ctx)
{
	mainloop_removewatch(ctx->mainloop, ctx->socket, FD_READ);
	close(ctx->socket);
	free(ctx);
}

void module_preact()
{
	if (
			regcomp(&get_match, "^GET \\(.*\\) HTTP.*$", REG_NEWLINE) != 0 ||
			regcomp(&header_match, "^\\(.*\\): \\(.*\\)\r$", REG_NEWLINE) != 0 ||
			regcomp(&params_match, "\\([^&]\\+\\)=\\([^&]\\+\\)", 0) != 0
	)
	{
		LOG(LOG_FATAL, "Could not compile regular expression to match HTTP headers!!");
	}
}
