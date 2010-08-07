#include <stdio.h>
#include <string.h>

#include <aul/log.h>

#include <kernel.h>
#include <service.h>
#include "internal.h"

extern GHashTable * service_table;

#define S_LIST_BUFSIZE		5000
#define S_LIST_CMDSIZE		20
#define S_LIST_CMDLIST		"list"
#define S_LIST_CMDSTART		"start="
#define S_LIST_CMDSTOP		"stop="


static void s_list_replyservices(service_h service_handle, stream_h stream_handle)
{
	char buf[S_LIST_BUFSIZE];
	size_t i=0;

	i += snprintf(buf + i, S_LIST_BUFSIZE - i, "<servicelist>");
	mutex_lock(&servicelock);
	{
		GHashTableIter itr;
		g_hash_table_iter_init(&itr, service_table);

		service_t * service = NULL;
		while (g_hash_table_iter_next(&itr, NULL, (void **)&service))
		{
			i += snprintf(buf + i, S_LIST_BUFSIZE - i, "<service id=\"%s\" name=\"%s\" format=\"%s\">", service->handle, service->name, service->format);
			if (service->desc != NULL)
			{
				i += snprintf(buf + i, S_LIST_BUFSIZE - i, "<description>%s</description>", service->desc);
			}

			//TODO - add params

			i += snprintf(buf + i, S_LIST_BUFSIZE - i, "</service>");
		}
	}
	mutex_unlock(&servicelock);
	i += snprintf(buf + i, S_LIST_BUFSIZE - i, "</servicelist>");

	service_writeclientdata(service_handle, stream_handle, kernel_timestamp(), buf, i);
}

static void s_list_userdata(service_h service, stream_h stream, uint64_t timestamp_us, const void * data, size_t length)
{
	char buf[S_LIST_CMDSIZE];
	ZERO(buf);

	memcpy(buf, data, MIN(S_LIST_CMDSIZE - 1, length));

	if (strcmp(buf, S_LIST_CMDLIST) == 0)
	{
		s_list_replyservices(service, stream);
	}
	else if (strprefix(buf, S_LIST_CMDSTART))
	{
		service_startstream(buf + strlen(S_LIST_CMDSTART), stream);
	}
	else if (strprefix(buf, S_LIST_CMDSTOP))
	{
		service_stopstream(buf + strlen(S_LIST_CMDSTOP), stream);
	}
}

static void s_alive_userdata(service_h service, stream_h stream, uint64_t timestamp_us, const void * data, size_t length)
{
	//do nothing
}

static void s_echo_userdata(service_h service, stream_h stream, uint64_t timestamp_us, const void * data, size_t length)
{
	service_writeclientdata(service, stream, kernel_timestamp(), data, length);
}


static void s_log_newconnect(service_h service, stream_h stream)
{
	const char * history = kernel_loghistory();
	service_writeclientdata(S_LOG_HANDLE, stream, kernel_timestamp(), history, strlen(history));
}

static void s_log_write(Level level, const char * domain, uint64_t milliseconds, const char * message, void * userdata)
{
	if (isunique(S_LOG_HANDLE, service_table))
	{
		//service doesn't exist anymore
		log_removelistener(s_log_write);
		return;
	}

	bool * ld = userdata;
	if (*ld)
	{
		//log has been disabled
		return;
	}

	//disable the log to prevent recusion on logged errors
	*ld = true;
	{
		String msg = kernel_logformat(level, domain, milliseconds, message);
		service_writedata(S_LOG_HANDLE, kernel_timestamp(), msg.string, msg.length);
	}
	*ld = false;
}

static void s_log_close(void * userdata)
{
	FREE(userdata);
}


void service_default_init()
{
	service_register(S_LIST_HANDLE, "List and reg utils", XML, NULL, "Manage service registrations", NULL, NULL, s_list_userdata);
	service_register(S_ALIVE_HANDLE, "Keep alive util", RAW, NULL, "Keep stream alive service, does not respond", NULL, NULL, s_alive_userdata);
	service_register(S_ECHO_HANDLE, "Echo data util", RAW, NULL, "Echo data back to client", NULL, NULL, s_echo_userdata);
	service_register(S_LOG_HANDLE, "Log messages", TXT, NULL, "Send log messages to client", s_log_newconnect, NULL, NULL);

	bool * ld = malloc0(sizeof(bool));
	log_addlistener(s_log_write, s_log_close, ld);
}

void service_startstream_default(stream_h stream_handle)
{
	service_startstream(S_LIST_HANDLE, stream_handle);
	service_startstream(S_ALIVE_HANDLE, stream_handle);
	service_startstream(S_ECHO_HANDLE, stream_handle);
}
