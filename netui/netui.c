#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdarg.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <microhttpd.h>

#include <glib.h>

#include <kernel.h>

#define ROOT		"netui/httpd"

#define ERROR_404	"<html><head><title>File not found</title></head><body><h1>Error 404</h1>File not found</body></html>"
#define ERROR_OPEN	"<html><head><title>Error: Open file</title></head><body><h1>Could not open file</h1></body></html>"

#define T_CALGET_HEAD	"Cal.rownum=0;Cal.loading=true;Cal.calTab.removeAll();Cal.calTab.add(["
#define T_CALGET_ITEM	"Cal.mkrow('%s', '%s', '%c', '%s', %lf, %lf, %lf, %lf),"
#define T_CALGET_TAIL	"]);Cal.calTab.doLayout(false, true);Cal.loading=false;"
#define T_CALSET		""
#define T_CALSAVE		"setTimeout(function(){Ext.getCmp('cal_comment').setValue('');Ext.MessageBox.hide();}, 900);"

int port = 80;

CFG_PARAM(port, "i", "Specifies the http port to connect over");
MOD_INIT(module_init);


static double serve_calget__step(double stride)
{
	if (stride <= 100.0)
	{
		return 1.0;
	}
	else if (stride <= 1000.0)
	{
		return 10.0;
	}
	else if (stride <= 20000.0)
	{
		return 100.0;
	}
	else
	{
		return 1000.0;
	}
}

static void serve_calget__itri(void * udata, char * module, char * name, char type, char * desc, int value, int min, int max)
{
	GString * str = udata;
	double stride = serve_calget__step(max-min);
	g_string_append_printf(str, T_CALGET_ITEM, module, name, type, desc, (double)value, (double)min, (double)max, stride);
}

static void serve_calget__itrd(void * udata, char * module, char * name, char type, char * desc, double value, double min, double max)
{
	GString * str = udata;
	double stride = serve_calget__step(max-min);
	g_string_append_printf(str, T_CALGET_ITEM, module, name, type, desc, value, min, max, stride);
}

static struct MHD_Response * serve_calget(GHashTable * get)
{
	char * revert = g_hash_table_lookup(get, "revert");
	if (revert != NULL && strcmp(revert, "1") == 0)
	{
		cal_revert();
	}

	GString * str = g_string_new(T_CALGET_HEAD);
	cal_iterate(serve_calget__itri, serve_calget__itrd, str);
	g_string_append_printf(str, T_CALGET_TAIL);

	char * calstr = g_string_free(str, FALSE);
	return MHD_create_response_from_data(strlen(calstr), calstr, MHD_YES, MHD_NO);
}

static struct MHD_Response * serve_calset(GHashTable * get)
{
	cal_setparam(g_hash_table_lookup(get, "module"), g_hash_table_lookup(get, "name"), g_hash_table_lookup(get, "value"));
	return MHD_create_response_from_data(strlen(T_CALSET), T_CALSET, MHD_NO, MHD_NO);
}

static struct MHD_Response * serve_calsave(GHashTable * get)
{
	cal_merge(g_hash_table_lookup(get, "comment"));
	return MHD_create_response_from_data(strlen(T_CALSAVE), T_CALSAVE, MHD_NO, MHD_NO);
}

static int serve_file__callback(void * userdata, uint64_t pos, char * buf, int max)
{
	int fp = (int)userdata;
	
	lseek(fp, pos, SEEK_SET);
	int r = read(fp, buf, max);
	
	return r <= 0 ? -1 : r;
}

static void serve_file__cleanup(void * userdata)
{
	close((int)userdata);
}


static struct MHD_Response * serve_file(const char * file)
{
	struct stat statbuf;
	
	if (strcmp(file, "/") == 0)
	{
		file = "/index.html";
	}
	
	char path[150];
	snprintf(path, 150, "%s/%s%s", INSTALL, ROOT, file);
	
	if (stat(path, &statbuf) == -1 || !S_ISREG(statbuf.st_mode))
	{
		return MHD_create_response_from_data(strlen(ERROR_404), ERROR_404, MHD_NO, MHD_NO);
	}
	
	int fp = open(path, O_RDONLY);
	if (fp < 0)
	{
		return MHD_create_response_from_data(strlen(ERROR_OPEN), ERROR_OPEN, MHD_NO, MHD_NO);
	}
	
	
	return MHD_create_response_from_callback(-1, 512, serve_file__callback, (void *)fp, serve_file__cleanup);
}


static int get(void * userdata, enum MHD_ValueKind kind, const char * key, const char * value)
{
	GHashTable * table = userdata;
	g_hash_table_insert(table, (char *)key, (char *)value);
	return MHD_YES;
}


static int new_client(void * cls, struct MHD_Connection * connection, const char * url, const char * method, const char * version, const char * upload_data, size_t * upload_data_size, void ** ptr)
{
	struct MHD_Response * response;
	int ret;

	if (0 != strcmp(method, "GET"))
		return MHD_NO;

	if (*ptr == NULL) 
	{
		*(int *)ptr = 1;
		return MHD_YES;
	}
	

	GHashTable * args = g_hash_table_new(g_str_hash, g_str_equal);
	MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, get, args);

	if (strcmp(url, "/calget") == 0)
	{
		response = serve_calget(args);
	}
	else if (strcmp(url, "/calset") == 0)
	{
		response = serve_calset(args);

	}
	else if (strcmp(url, "/calsave") == 0)
	{
		response = serve_calsave(args);
	}
	else
	{
		response = serve_file(url);
	}
	
	ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);
	g_hash_table_destroy(args);
	
	return ret;
}


void module_init()
{
	MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, port, NULL, NULL, new_client, NULL, MHD_OPTION_END);
}

