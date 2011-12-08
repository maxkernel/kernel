#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <aul/string.h>

#include <kernel.h>
#include <httpserver.h>

#define ROOT		"netui/httpd"

#define T_CALGET_HEAD	"Cal.rownum=0;Cal.loading=true;Cal.calTab.removeAll();Cal.calTab.add(["
#define T_CALGET_ITEM	"Cal.mkrow('%s', '%s', '%c', '%s', %lf, %lf, %lf, %lf),"
#define T_CALGET_TAIL	"]);Cal.calTab.doLayout(false, true);Cal.loading=false;"
#define T_CALSET		" "
#define T_CALSAVE		"setTimeout(function(){Ext.getCmp('cal_comment').setValue('');Ext.MessageBox.hide();}, 900);"

int port = 80;

CFG_PARAM(port, "i", "Specifies the http port to connect over");
MOD_INIT(module_init);
MOD_DESTROY(module_destroy);
MOD_DEPENDENCY("httpserver");


static http_context * ctx;

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

static void handle_root(http_connection * conn, http_context * ctx, const char * uri)
{
	struct stat statbuf;

	if (strcmp(uri, "/") == 0)
	{
		uri = "/index.html";
	}

	string_t path = string_new("%s/%s%s", INSTALL, ROOT, uri);
	if (stat(path.string, &statbuf) == -1 || !S_ISREG(statbuf.st_mode))
	{
		http_printf(conn, "HTTP/1.1 404 Not Found\r\n\r\n");
		return;
	}

	int fp = open(path.string, O_RDONLY);
	if (fp < 0)
	{
		// We could stat the file but couldn't open it? How can this be?
		http_printf(conn, "HTTP/1.1 500 Internal Server Error\r\n\r\n");
		return;
	}


	// If we are this far, we are successful at opening file
	http_printf(conn, "HTTP/1.1 200 OK\r\n\r\n");

	ssize_t bytesread;
	char buf[150];
	while ((bytesread = read(fp, buf, sizeof(buf))) > 0)
	{
		http_write(conn, buf, bytesread);
	}
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

	http_adduri(ctx, "/calget", MATCH_ALL, handle_calget, NULL);
	http_adduri(ctx, "/calset", MATCH_ALL, handle_calset, NULL);
	http_adduri(ctx, "/calsave", MATCH_ALL, handle_calsave, NULL);
	http_adduri(ctx, "/", MATCH_PREFIX, handle_root, NULL);
}

void module_destroy()
{
	http_destroy(ctx);
}
