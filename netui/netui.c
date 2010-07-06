#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <kernel.h>
#include <httpserver.h>

#define ROOT		"netui/httpd"

#define T_CALGET_HEAD	"Cal.rownum=0;Cal.loading=true;Cal.calTab.removeAll();Cal.calTab.add(["
#define T_CALGET_ITEM	"Cal.mkrow('%s', '%s', '%c', '%s', %lf, %lf, %lf, %lf),"
#define T_CALGET_TAIL	"]);Cal.calTab.doLayout(false, true);Cal.loading=false;"
#define T_CALSET		""
#define T_CALSAVE		"setTimeout(function(){Ext.getCmp('cal_comment').setValue('');Ext.MessageBox.hide();}, 900);"

int port = 80;

CFG_PARAM(port, "i", "Specifies the http port to connect over");
MOD_INIT(module_init);
MOD_DESTROY(module_destroy);
MOD_DEPENDENCY("httpserver");


static http_context * ctx;

static void handle_calget(http_connection * conn, hashtable_t * headers, hashtable_t * params, const char * uri)
{

}

static void handle_calset(http_connection * conn, hashtable_t * headers, hashtable_t * params, const char * uri)
{

}

static void handle_calsave(http_connection * conn, hashtable_t * headers, hashtable_t * params, const char * uri)
{

}

static void handle_root(http_connection * conn, hashtable_t * headers, hashtable_t * params, const char * uri)
{
	struct stat statbuf;

	if (strcmp(uri, "/") == 0)
	{
		uri = "/index.html";
	}

	String path = string_new("%s/%s%s", INSTALL, ROOT, uri);
	if (stat(path.string, &statbuf) == -1 || !S_ISREG(statbuf.st_mode))
	{
		http_printf(conn, "HTTP/1.0 404 Not Found\r\n\r\n");
		return;
	}

	int fp = open(path.string, O_RDONLY);
	if (fp < 0)
	{
		http_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
					"<html><body><h1>Error: Could not open file %s</h1>"
					"</body></html>", uri);
		return;
	}

	ssize_t bytesread;
	char buf[150];
	while ((bytesread = read(fp, buf, sizeof(buf))) > 0)
	{
		http_write(conn, buf, bytesread);
	}
}

void module_init()
{
	Error * err = NULL;

	ctx = http_new(port, NULL, &err);
	if (err != NULL)
	{
		LOG(LOG_ERR, "Could not create netui HTTP server on port %d: %s", port, err->message.string);
		error_free(err);
		return;
	}

	http_adduri(ctx, "/calget", MATCH_ALL, handle_calget, NULL);
	http_adduri(ctx, "/calset", MATCH_ALL, handle_calset, NULL);
	http_adduri(ctx, "/calsave", MATCH_ALL, handle_calsave, NULL);
	http_adduri(ctx, "/", MATCH_PREFIX, handle_root, NULL);
}

void module_destroy()
{
	http_destroy(ctx);
}
