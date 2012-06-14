#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <aul/string.h>

#include <kernel.h>
#include <httpserver.h>


#define ROOT		"netui/www"


int port = 80;

static httpcontext_t * ctx;

void handle_get(httpconnection_t * conn, httpcontext_t * ctx, const char * uri);
void handle_set(httpconnection_t * conn, httpcontext_t * ctx, const char * uri);

static void reply_file(httpconnection_t * conn, const char * path, const char * mimetype, const char * headers)
{
	struct stat statbuf;

	if (stat(path, &statbuf) == -1 || !S_ISREG(statbuf.st_mode))
	{
		http_printf(conn, "HTTP/1.1 404 Not Found\r\n\r\n");
		return;
	}

	int fp = open(path, O_RDONLY);
	if (fp < 0)
	{
		// We could stat the file but couldn't open it? How can this be?
		http_printf(conn, "HTTP/1.1 500 Internal Server Error\r\n\r\n");
		return;
	}


	// If we are this far, we are successful at opening file
	http_printf(conn, "HTTP/1.1 200 OK\r\n");
	if (mimetype != NULL)
	{
		http_printf(conn, "Content-Type: %s\r\n", mimetype);
	}
	if (headers != NULL)
	{
		http_printf(conn, "%s", headers);
	}
	http_printf(conn, "\r\n");


	ssize_t bytesread;
	char buf[150];
	while ((bytesread = read(fp, buf, sizeof(buf))) > 0)
	{
		http_write(conn, buf, bytesread);
	}

	close(fp);
}

static const char * reply_mimetype(const char * path)
{
	if (strsuffix(path, ".js"))										return "application/javascript";
	else if (strsuffix(path, ".json"))								return "application/json";
	else if (strsuffix(path, ".css"))								return "text/css";
	else if (strsuffix(path, ".html"))								return "text/html";
	else if (strsuffix(path, ".gif"))								return "image/gif";
	else if (strsuffix(path, ".png"))								return "image/png";
	else if (strsuffix(path, ".jpeg") || strsuffix(path, ".jpg"))	return "image/jpeg";
	else if (strsuffix(path, ".ttf"))								return "application/x-font-ttf";
	else if (strsuffix(path, ".eot"))								return "application/vnd.ms-fontobject";

	return NULL;
}

static void handle_root(httpconnection_t * conn, httpcontext_t * ctx, const char * uri)
{
	if (strcmp(uri, "/") == 0)
	{
		uri = "/index.html";
	}

	string_t path = string_new("%s/%s%s", INSTALL, ROOT, uri);
	reply_file(conn, path.string, reply_mimetype(uri), NULL);
}

static void handle_compressed(httpconnection_t * conn, httpcontext_t * ctx, const char * uri)
{
	string_t path = string_new("%s/%s%s.gz", INSTALL, ROOT, uri);
	reply_file(conn, path.string, reply_mimetype(uri), "Content-Encoding: gzip\r\n");
}

bool module_init()
{
	exception_t * e = NULL;

	ctx = http_new(port, NULL, &e);
	if (ctx == NULL || exception_check(&e))
	{
		LOG(LOG_ERR, "Could not create netui HTTP server on port %d: %s", port, exception_message(e));
		exception_free(e);
		return true;
	}

	http_adduri(ctx, "/get/", MATCH_PREFIX, handle_get, NULL);
	http_adduri(ctx, "/set/", MATCH_PREFIX, handle_set, NULL);
	http_adduri(ctx, "/extjs/ext-all.js", MATCH_ALL, handle_compressed, NULL);
	http_adduri(ctx, "/extjs/ext-all-debug.js", MATCH_ALL, handle_compressed, NULL);
	http_adduri(ctx, "/extjs/resources/css/ext-all.css", MATCH_ALL, handle_compressed, NULL);
	http_adduri(ctx, "/", MATCH_PREFIX, handle_root, NULL);

	return true;
}

void module_destroy()
{
	if (ctx != NULL)
	{
		http_destroy(ctx);
	}
}


module_name("NetUI");
module_version(1,0,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("A helpful HTTP server to introspect kernel configuration and calibration");
module_dependency("httpserver");
module_oninitialize(module_init);
module_ondestroy(module_destroy);

module_config(port, 'i', "The HTTP port to allow connections over");
