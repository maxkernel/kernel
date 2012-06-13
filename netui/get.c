#include <aul/constraint.h>
#include <aul/string.h>

#include <kernel.h>
#include <httpserver.h>

#define KOBJ_DESC_BUFFSIZE		1024

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

			http_printf(conn, "{'name':'%s','subname':'%s','sig':'%c','value':'%s','step':%f,'desc':'%s'},", (domain == NULL)? "" : domain, name, sig, value, step, desc);
		}
	}
	http_printf(conn, "]}");
}

static void get_objects(httpconnection_t * conn, httpcontext_t * ctx)
{
	http_printf(conn, "HTTP/1.1 200 OK\r\n");
	http_printf(conn, "Content-Type: application/json\r\n");
	http_printf(conn, "\r\n");

	http_printf(conn, "{'result':[");
	{
		iterator_t kitr = kobj_itr();
		{
			const kobject_t * kobj = NULL;
			while (kobj_next(kitr, &kobj))
			{
				char buffer[KOBJ_DESC_BUFFSIZE] = {0};
				ssize_t wrote = kobj_desc(kobj, buffer, KOBJ_DESC_BUFFSIZE);

				if (wrote < 0)
				{
					strcpy(buffer, "{ 'error': 'could not retrieve data' }");
					wrote = strlen(buffer);
				}
				else if (wrote == 0)
				{
					strcpy(buffer, "{ 'error': 'no data' }");
					wrote = strlen(buffer);
				}

				string_t parent = string_blank();
				if (kobj_parent(kobj) != NULL)
				{
					string_set(&parent, "%#x", kobj_id(kobj_parent(kobj)));
				}

				http_printf(conn, "{ 'name': '%s', 'subname': '%s', 'id': '%#x', 'parent': '%s', 'desc': \"", kobj_classname(kobj), kobj_objectname(kobj), kobj_id(kobj), parent.string);
				http_write(conn, buffer, wrote);
				http_printf(conn, "\"},");
			}
		}
		iterator_free(kitr);
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

	else if (strsuffix(uri, "/objects.json"))
	{
		get_objects(conn, ctx);
	}

	else
	{
		http_printf(conn, "HTTP/1.1 404 Not Found\r\n\r\n");
	}
}
