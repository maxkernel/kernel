#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <aul/string.h>

#include <kernel.h>
#include <httpserver.h>

int port = 80;

CFG_PARAM(port, "i", "Specifies the http port to connect over");
MOD_INIT(module_init);
MOD_DESTROY(module_destroy);
MOD_DEPENDENCY("httpserver");

#define ROOT		"netui/www"

static http_context * ctx;

void handle_get(http_connection * conn, http_context * ctx, const char * uri);
void handle_set(http_connection * conn, http_context * ctx, const char * uri);

static void reply_file(http_connection * conn, const char * path, const char * mimetype, const char * headers)
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

#if 0
static inline double handle_calget__step(double stride)
{
	if (stride <= 100.0)			return 1.0;
	else if (stride <= 1000.0)		return 10.0;
	else if (stride <= 20000.0)		return 100.0;
	else							return 1000.0;
}

static void handle_calget__itri(void * udata, char * module, char * name, char type, char * desc, int value, int min, int max)
{
	http_connection * conn = udata;
	double stride = handle_calget__step(max-min);
	http_printf(conn, T_CALGET_ITEM, module, name, type, desc, (double)value, (double)min, (double)max, stride);
}

static void handle_calget__itrd(void * udata, char * module, char * name, char type, char * desc, double value, double min, double max)
{
	http_connection * conn = udata;
	double stride = handle_calget__step(max-min);
	http_printf(conn, T_CALGET_ITEM, module, name, type, desc, value, min, max, stride);
}

static void handle_calget(http_connection * conn, http_context * ctx, const char * uri)
{
	const char * revert = http_getparam(ctx, "revert");
	if (revert != NULL && strcmp(revert, "1") == 0)
	{
		cal_revert();
	}

	http_printf(conn, "HTTP/1.1 200 OK\r\n\r\n");
	http_printf(conn, T_CALGET_HEAD);

	cal_iterate(handle_calget__itri, handle_calget__itrd, conn);

	http_printf(conn, T_CALGET_TAIL);
}

static void handle_calset(http_connection * conn, http_context * ctx, const char * uri)
{
	cal_setparam(http_getparam(ctx, "module"), http_getparam(ctx, "name"), http_getparam(ctx, "value"));
	http_printf(conn, "HTTP/1.1 200 OK\r\n\r\n");
	http_printf(conn, T_CALSET);
}

static void handle_calsave(http_connection * conn, http_context * ctx, const char * uri)
{
	cal_merge(http_getparam(ctx, "comment"));
	http_printf(conn, "HTTP/1.1 200 OK\r\n\r\n");
	http_printf(conn, T_CALSAVE);
}
#endif

static void handle_root(http_connection * conn, http_context * ctx, const char * uri)
{
	if (strcmp(uri, "/") == 0)
	{
		uri = "/index.html";
	}

	string_t path = string_new("%s/%s%s", INSTALL, ROOT, uri);
	reply_file(conn, path.string, reply_mimetype(uri), NULL);
}

static void handle_compressed(http_connection * conn, http_context * ctx, const char * uri)
{
	string_t path = string_new("%s/%s%s.gz", INSTALL, ROOT, uri);
	reply_file(conn, path.string, reply_mimetype(uri), "Content-Encoding: gzip\r\n");
}

void module_init()
{
	exception_t * err = NULL;

	ctx = http_new(port, NULL, &err);
	if (err != NULL)
	{
		LOG(LOG_ERR, "Could not create netui HTTP server on port %d: %s", port, err->message);
		exception_free(err);
		return;
	}

	http_adduri(ctx, "/get/", MATCH_PREFIX, handle_get, NULL);
	http_adduri(ctx, "/set/", MATCH_PREFIX, handle_set, NULL);
	http_adduri(ctx, "/extjs/ext-all.js", MATCH_ALL, handle_compressed, NULL);
	http_adduri(ctx, "/extjs/ext-all-debug.js", MATCH_ALL, handle_compressed, NULL);
	http_adduri(ctx, "/extjs/resources/css/ext-all.css", MATCH_ALL, handle_compressed, NULL);
	http_adduri(ctx, "/", MATCH_PREFIX, handle_root, NULL);
}

void module_destroy()
{
	if (ctx != NULL)
	{
		http_destroy(ctx);
	}
}
