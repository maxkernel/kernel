#include <aul/constraint.h>
#include <aul/string.h>

#include <kernel.h>
#include <httpserver.h>


static void get_calibration_mode(httpconnection_t * conn, httpcontext_t * ctx)
{
	http_printf(conn, "HTTP/1.1 200 OK\r\n");
	http_printf(conn, "Content-Type: application/json\r\n");
	http_printf(conn, "\r\n");

	const char * mode = NULL;
	switch (cal_getmode())
	{
		case calmode_runtime:
			mode = "runtime";
			break;

		case calmode_calibrating:
			mode = "calibrating";
			break;
	}

	http_printf(conn, "OK\n%s", mode);
}

static void get_calibration(httpconnection_t * conn, httpcontext_t * ctx)
{
	http_printf(conn, "HTTP/1.1 200 OK\r\n");
	http_printf(conn, "Content-Type: application/json\r\n");
	http_printf(conn, "\r\n");

	http_printf(conn, "{'result':[");
	{
		iterator_t itr = cal_iterator();

		const char * domain = NULL, * name = NULL, *desc = NULL, * value = NULL;
		char sig = 0;
		const constraint_t * constraint = NULL;
		while (cal_next(itr, &domain, &name, &sig, &constraint, &desc, &value))
		{
			double step = 1.0;
			constraint_getstep(constraint, &step);

			http_printf(conn, "{'domain':'%s','name':'%s','sig':'%c','value':'%s','step':%f,'desc':'%s'},", (domain == NULL)? "" : domain, name, sig, value, step, desc);
		}
	}
	http_printf(conn, "]}");
}


void handle_get(httpconnection_t * conn, httpcontext_t * ctx, const char * uri)
{
	if (strsuffix(uri, "/calibration/mode"))
	{
		get_calibration_mode(conn, ctx);
	}
	else if (strsuffix(uri, "/calibration.json"))
	{
		get_calibration(conn, ctx);
	}
	else
	{
		http_printf(conn, "HTTP/1.1 404 Not Found\r\n\r\n");
	}
}
