#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <regex.h>

#include "httpserver.h"
#include <aul/net.h>
#include <aul/list.h>
#include <kernel.h>

#define NUM_BUFFERS		10
#define NUM_KEYVALUES	30
#define BUFFER_LEN		1000

static regex_t get_match;
static regex_t params_match;
static regex_t header_match;

typedef struct
{
	const char * key;
	const char * value;
	hashentry_t hash_entry;
} httpkeyvalue_t;

typedef struct
{
	const char * uri;
	httpmatch_t match;
	httpcallback_f callback;
	void * userdata;

	list_t filter_list;
} httpfilter_t;

typedef struct
{
	bool inuse;
	fdwatcher_t watcher;
	char buffer[BUFFER_LEN];
	size_t length;

	httpcontext_t * ctx;
} httpbuffer_t;

struct __httpcontext_t
{
	fdwatcher_t watcher;
	list_t filters;

	httpbuffer_t buffers[NUM_BUFFERS];
	httpkeyvalue_t keyvalues[NUM_KEYVALUES];
	hashtable_t headers;
	hashtable_t parameters;
};

static inline string_t addr2string(uint32_t ip)
{
	unsigned char a4 = (ip & 0xFF000000) >> 24;
	unsigned char a3 = (ip & 0x00FF0000) >> 16;
	unsigned char a2 = (ip & 0x0000FF00) >> 8;
	unsigned char a1 = ip & 0xFF;
	return string_new("%d.%d.%d.%d", a1, a2, a3, a4);
}

void http_urldecode(char * string)
{
	char * i = NULL;
	while ((i = strchr(string, '%')) != NULL)
	{
		size_t len = 0;
		if ((len = strlen(i)) < 3)
		{
			break;
		}

		// Parse the value
		const char x[] = { i[1], i[2], '\0' };
		long v = strtol(x, NULL, 16);

		// Collapse the string
		memcpy(&i[1], &i[3], len-2);

		// Set the value
		i[0] = (char)v;
	}
}

static bool http_newdata(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{
	httpbuffer_t * buffer = userdata;

	ssize_t bytesread = recv(fd, buffer->buffer + buffer->length, BUFFER_LEN - buffer->length - 1, 0);
	if (bytesread > 0)
	{
		buffer->length += bytesread;
		buffer->buffer[buffer->length] = '\0';

		if (strstr(buffer->buffer, "\r\n\r\n") != NULL)
		{
			// We have the full header, parse it
			regmatch_t match[3];
			memset(match, 0, sizeof(regmatch_t) * 3);

			if (regexec(&get_match, buffer->buffer, 3, match, 0) == 0)
			{
				// The browser is using GET. Gooood.
				char request_uri[250] = {0};
				memcpy(request_uri, buffer->buffer + match[1].rm_so, min(sizeof(request_uri) - 1, match[1].rm_eo - match[1].rm_so));

				size_t keyvalue_index = 0;

				// Parse the GET parameters off the URI
				hashtable_init(&buffer->ctx->parameters, hash_str, hash_streq);
				if (strchr(request_uri, '?') != NULL)
				{
					char * params = strchr(request_uri, '?');
					*params++ = '\0';

					size_t offset = 0;
					while (regexec(&params_match, params + offset, 3, match, 0) == 0 && keyvalue_index < NUM_KEYVALUES)
					{
						bool atend = params[offset + match[0].rm_eo] == '\0';

						char * key = params + offset + match[1].rm_so;
						key[match[1].rm_eo - match[1].rm_so] = '\0';

						char * value = params + offset + match[2].rm_so;
						value[match[2].rm_eo - match[2].rm_so] = '\0';

						http_urldecode(key);
						http_urldecode(value);

						httpkeyvalue_t * kv = &buffer->ctx->keyvalues[keyvalue_index++];
						kv->key = key;
						kv->value = value;
						hashtable_put(&buffer->ctx->parameters, key, &kv->hash_entry);

						if (atend)
							break;
						else
							offset += match[0].rm_eo+1;
					}
				}

				// Pass the request through the filters
				httpfilter_t * pass_match = NULL;
				list_t * pos;
				list_foreach(pos, &buffer->ctx->filters)
				{
					httpfilter_t * filter = list_entry(pos, httpfilter_t, filter_list);
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
					//hashtable_t * headers = &buffer->ctx->headers;
					hashtable_init(&buffer->ctx->headers, hash_str, hash_streq);

					size_t offset = 0;
					while (regexec(&header_match, buffer->buffer + offset, 3, match, 0) == 0 && keyvalue_index < NUM_KEYVALUES)
					{
						char * key = buffer->buffer + offset + match[1].rm_so;
						key[match[1].rm_eo - match[1].rm_so] = '\0';

						char * value = buffer->buffer + offset + match[2].rm_so;
						value[match[2].rm_eo - match[2].rm_so] = '\0';

						httpkeyvalue_t * kv = &buffer->ctx->keyvalues[keyvalue_index++];
						kv->key = key;
						kv->value = value;
						hashtable_put(&buffer->ctx->headers, key, &kv->hash_entry);

						offset += match[0].rm_eo+1;
					}

					if (keyvalue_index == NUM_KEYVALUES)
					{
						// We (most likely) ran out of http_keyvalues before completed parsing. Log it
						LOG(LOG_WARN, "Ran out of key->value buffers in http_newdata. Consider increasing the number of key->value buffers (currently %d)", NUM_KEYVALUES);
					}

					pass_match->callback(&fd, buffer->ctx, request_uri);
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
	return false;
}

static bool http_newclient(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{
	labels(end);

	httpcontext_t * ctx = userdata;

	struct sockaddr_in addr;
	socklen_t socklen = sizeof(addr);
	int sock = accept(fd, (struct sockaddr *)&addr, &socklen);
	if (sock < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			LOG(LOG_WARN, "Could not accept new http client socket: %s", strerror(errno));
		}
		goto end;
	}

	// TODO IMPORTANT - make sock non-blocking

	// TODO - add lock around getting free buffers

	httpbuffer_t * buffer = NULL;
	for (size_t i = 0; i < NUM_BUFFERS; i++)
	{
		if (!ctx->buffers[i].inuse)
		{
			buffer = &ctx->buffers[i];
			break;
		}
	}

	if (buffer == NULL)
	{
		LOG(LOG_WARN, "Could not accept new HTTP client: Out of HTTP buffers!");
		close(sock);
		goto end;
	}
	// Set up the buffer
	buffer->inuse = true;
	watcher_newfd(&buffer->watcher, sock, FD_READ, http_newdata, buffer);
	buffer->buffer[0] = '\0';
	buffer->length = 0;
	buffer->ctx = ctx;

	// Now set up the watch on the accept'd file descriptor
	exception_t * e = NULL;
	if (!mainloop_addwatcher(loop, &buffer->watcher, &e) || exception_check(&e))
	{
		LOG(LOG_WARN, "Could not add http client fd to mainloop: %s", exception_message(e));
		exception_free(e);

		// Remove the in-use flag to make it available again
		watcher_close(&buffer->watcher);
		buffer->inuse = false;
		goto end;
	}

end:
	return true;
}

httpcontext_t * http_new(uint16_t port, mainloop_t * mainloop, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if (mainloop == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}

	int sock = tcp_server(port, err);
	if (sock == -1 || exception_check(err))
	{
		return NULL;
	}

	// TODO IMPORTANT - make socket non-blocking

	httpcontext_t * ctx = malloc(sizeof(httpcontext_t));
	memset(ctx, 0, sizeof(httpcontext_t));
	watcher_newfd(&ctx->watcher, sock, FD_READ, http_newclient, ctx);
	list_init(&ctx->filters);

	if (!mainloop_addwatcher(mainloop, &ctx->watcher, err) || exception_check(err))
	{
		http_destroy(ctx);
		return NULL;
	}

	return ctx;
}

void http_adduri(httpcontext_t * ctx, const char * uri, httpmatch_t match, httpcallback_f cb, void * userdata)
{
	httpfilter_t * filt = malloc(sizeof(httpfilter_t));
	memset(filt, 0, sizeof(httpfilter_t));
	filt->uri = uri;
	filt->match = match;
	filt->callback = cb;
	filt->userdata = userdata;

	list_add(&ctx->filters, &filt->filter_list);
}

void http_printf(httpconnection_t * conn, const char * fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	http_vprintf(conn, fmt, args);
	va_end(args);
}

void http_vprintf(httpconnection_t * conn, const char * fmt, va_list args)
{
	string_t msg = string_blank();
	string_vappend(&msg, fmt, args);
	http_write(conn, msg.string, msg.length);
}

void http_write(httpconnection_t * conn, const void * buf, size_t len)
{
	send(*conn, buf, len, 0);
}

const char * http_getheader(httpcontext_t * ctx, const char * name)
{
	hashentry_t * entry = hashtable_get(&ctx->headers, name);
	if (entry != NULL)
	{
		httpkeyvalue_t * kv = hashtable_entry(entry, httpkeyvalue_t, hash_entry);
		return kv->value;
	}

	return NULL;
}

const char * http_getparam(httpcontext_t * ctx, const char * name)
{
	hashentry_t * entry = hashtable_get(&ctx->parameters, name);
	if (entry != NULL)
	{
		httpkeyvalue_t * kv = hashtable_entry(entry, httpkeyvalue_t, hash_entry);
		return kv->value;
	}

	return NULL;
}

void http_destroy(httpcontext_t * ctx)
{
	// Remove http ctx watcher
	{
		exception_t * e = NULL;
		if (!mainloop_removewatcher(&ctx->watcher, &e) || exception_check(&e))
		{
			LOG(LOG_WARN, "Could not remove http ctx watcher: %s", exception_message(e));
			exception_free(e);
		}
	}

	// Free all the filters
	{
		list_t * pos = NULL, * n = NULL;
		list_foreach_safe(pos, n, &ctx->filters)
		{
			httpfilter_t * filt = list_entry(pos, httpfilter_t, filter_list);
			list_remove(&filt->filter_list);
			free(filt);
		}
	}

	// Free the entire struct
	free(ctx);
}

static void http_preactivate()
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


module_name("HTTP Server");
module_version(1,0,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Provides a simple (GET only) HTTP server. Intended to be used by other modules (not included directly)");
module_onpreactivate(http_preactivate);
