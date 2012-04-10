

#include <kernel.h>
#include <httpserver.h>


static void set_calibration_start(http_connection * conn, http_context * ctx)
{
	http_printf(conn, "HTTP/1.1 200 OK\r\n");
	http_printf(conn, "Content-Type: text/plain\r\n");
	http_printf(conn, "\r\n");
	cal_start();
	http_printf(conn, "OK");
}

static void set_calibration_preview(http_connection * conn, http_context * ctx)
{
	http_printf(conn, "HTTP/1.1 200 OK\r\n");
	http_printf(conn, "Content-Type: text/plain\r\n");
	http_printf(conn, "\r\n");

	const char * domain = http_getparam(ctx, "domain");
	if (domain != NULL && strlen(domain) == 0)
	{
		domain = NULL;
	}
	const char * name = http_getparam(ctx, "name");
	const char * value = http_getparam(ctx, "value");

	//LOG(LOG_DEBUG, "Preview calibration entry %s in domain %s with value %s", name, (domain == NULL)? "(none)" : domain, value);

	string_t hint = string_blank();
	const char * newvalue = cal_preview(domain, name, value, hint.string, string_available(&hint));
	if (newvalue != NULL)
	{
		http_printf(conn, "OK\n%s\n%s", newvalue, hint.string);
	}
	else
	{
		http_printf(conn, "Failure to preview calibration value");
	}
}

static void set_calibration_commit(http_connection * conn, http_context * ctx)
{
	http_printf(conn, "HTTP/1.1 200 OK\r\n");
	http_printf(conn, "Content-Type: text/plain\r\n");
	http_printf(conn, "\r\n");

	const char * comment = http_getparam(ctx, "comment");
	cal_commit(comment);
	http_printf(conn, "OK");
}

static void set_calibration_revert(http_connection * conn, http_context * ctx)
{
	http_printf(conn, "HTTP/1.1 200 OK\r\n");
	http_printf(conn, "Content-Type: text/plain\r\n");
	http_printf(conn, "\r\n");
	cal_revert();
	http_printf(conn, "OK");
}

void handle_set(http_connection * conn, http_context * ctx, const char * uri)
{
	if (strsuffix(uri, "/calibration/start"))
	{
		set_calibration_start(conn, ctx);
	}
	else if (strsuffix(uri, "/calibration/preview"))
	{
		set_calibration_preview(conn, ctx);
	}
	else if (strsuffix(uri, "/calibration/commit"))
	{
		set_calibration_commit(conn, ctx);
	}
	else if (strsuffix(uri, "/calibration/revert"))
	{
		set_calibration_revert(conn, ctx);
	}
	else
	{
		http_printf(conn, "HTTP/1.1 404 Not Found\r\n\r\n");
	}
}

