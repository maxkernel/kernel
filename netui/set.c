

#include <kernel.h>
#include <httpserver.h>


static void set_calibration_preview(http_connection * conn, http_context * ctx)
{
	http_printf(conn, "HTTP/1.1 200 OK\r\n");
	http_printf(conn, "Content-Type: text/plain\r\n");
	http_printf(conn, "\r\n");

	LOG(LOG_DEBUG, "Preview calibration entry %s with value %s", http_getparam(ctx, "name"), http_getparam(ctx, "value"));
	cal_setparam(http_getparam(ctx, "module"), http_getparam(ctx, "name"), http_getparam(ctx, "value"));
	http_printf(conn, "OK");
}

static void set_calibration_commit(http_connection * conn, http_context * ctx)
{
	http_printf(conn, "HTTP/1.1 200 OK\r\n");
	http_printf(conn, "Content-Type: text/plain\r\n");
	http_printf(conn, "\r\n");

	LOG(LOG_INFO, "Committing calibration changes with comment '%s'", http_getparam(ctx, "comment"));
	cal_merge(http_getparam(ctx, "comment"));
	http_printf(conn, "OK");
}

static void set_calibration_revert(http_connection * conn, http_context * ctx)
{
	http_printf(conn, "HTTP/1.1 200 OK\r\n");
	http_printf(conn, "Content-Type: text/plain\r\n");
	http_printf(conn, "\r\n");

	LOG(LOG_DEBUG, "Reverting calibration changes");
	// TODO - revert calibration. Nothing to do now... but later...
	http_printf(conn, "OK");
}

void handle_set(http_connection * conn, http_context * ctx, const char * uri)
{
	if (strsuffix(uri, "/calibration/preview"))
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

