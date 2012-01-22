#include <aul/string.h>

#include <kernel.h>
#include <httpserver.h>


static void calget__itri(void * udata, char * module, char * name, char type, char * desc, int value, int min, int max)
{
	http_connection * conn = udata;

	char * group = strrchr(module, '/');
	if (group == NULL)
	{
		group = module;
	}

	http_printf(conn, "{'group':'%s','name':'%s','type':'%c','value':'%d','min':'%d','max':'%d','desc':'%s'},", group, name, T_INTEGER, value, min, max, desc);
}

static void calget__itrd(void * udata, char * module, char * name, char type, char * desc, double value, double min, double max)
{
	http_connection * conn = udata;

	char * group = strrchr(module, '/');
	if (group == NULL)
	{
		group = module;
	}

	http_printf(conn, "{'group':'%s','name':'%s','type':'%c','value':'%f','min':'%f','max':'%f','desc':'%s'},", group, name, T_DOUBLE, value, min, max, desc);
}

static void get_calibration(http_connection * conn, http_context * ctx)
{
	http_printf(conn, "HTTP/1.1 200 OK\r\n");
	http_printf(conn, "Content-Type: application/json\r\n");
	http_printf(conn, "\r\n");

	http_printf(conn, "{'result':[");
	cal_iterate(calget__itri, calget__itrd, conn);
	http_printf(conn, "]}");
}


void handle_get(http_connection * conn, http_context * ctx, const char * uri)
{
	if (strsuffix(uri, "/calibration.json"))
	{
		get_calibration(conn, ctx);
	}
	else
	{
		http_printf(conn, "HTTP/1.1 404 Not Found\r\n\r\n");
	}
}
