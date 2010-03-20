#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "max.h"
#include "serialize.h"

static GHashTable * parsesyscalls(gchar * str)
{
	GHashTable * syscalls = g_hash_table_new(g_str_hash, g_str_equal);

	GError * err = NULL;
	GRegex * regex = g_regex_new("^([[:alpha:]_]+)\\(([:vildcsbILD]+)\\)$", G_REGEX_OPTIMIZE, 0, &err);
	if (regex == NULL)
	{
		g_error("Regex failed to compile: %s", err->message);
		return NULL;
	}

	gchar ** list = g_strsplit(str, "\n", 0);
	gint i=0;
	while (list[i] != NULL)
	{
		if (strlen(list[i]) == 0)
		{
			i++;
			continue;
		}

		GMatchInfo * info;
		g_regex_match(regex, list[i], 0, &info);

		if (g_match_info_matches(info))
		{
			gchar * name = g_match_info_fetch(info, 1);
			gchar * sig = g_match_info_fetch(info, 2);
			g_hash_table_insert(syscalls, name, sig);
		}
		else
		{
			g_warning("Syscall does not match regex: %s", list[i]);
		}

		i++;
	}
	
	return syscalls;
}

int main(int argc, char ** argv)
{
	if (argc < 2)
	{
		g_print("Nothing to do.\n");
		return 0;
	}

	maxhandle_t * hand = max_local();
	
	char * syscalls_str;
	if (max_syscall(hand, "syscall_list", "s:v", &syscalls_str) != 0)
	{
		g_error("Could not call kernel max_syscalls function");
		return 1;
	}
	
	GHashTable * syscalls = parsesyscalls(syscalls_str);
	GPtrArray * args = g_ptr_array_new();

	char * sig = g_hash_table_lookup(syscalls, argv[1]);
	if (sig == NULL)
	{
		g_error("Syscall %s does not exist in kernel", argv[1]);
		return 1;
	}
	
	const char * s = method_params(sig);
	if (argc-2 < strlen(s))
	{
		g_error("Invalid argument length for syscall %s with signature %s", argv[1], s);
	}

	int index = 0;
	for (; index < strlen(s); index++)
	{
		switch (s[index])
		{
			case T_BOOLEAN:
			{
				boolean * v = g_malloc(sizeof(boolean));
				*v = atoi(argv[index + 2]);
				g_ptr_array_add(args, v);
				break;
			}

			case T_INTEGER:
			{
				int * v = g_malloc(sizeof(int));
				*v = atoi(argv[index + 2]);
				g_ptr_array_add(args, v);
				break;
			}

			case T_DOUBLE:
			{
				double * v = g_malloc(sizeof(double));
				*v = g_strtod(argv[index + 2], NULL);
				g_ptr_array_add(args, v);
				break;
			}

			case T_CHAR:
			{
				char * v = g_malloc(sizeof(char));
				*v = argv[index + 2][0];
				g_ptr_array_add(args, v);
				break;
			}

			case T_STRING:
			{
				char ** v = g_malloc(sizeof(char **));
				*v = argv[index + 2];
				g_ptr_array_add(args, v);
				break;
			}

			case T_ARRAY_BOOLEAN:
			case T_ARRAY_INTEGER:
			case T_ARRAY_DOUBLE:
			{
				g_error("Cannot handle arrays");
				return 1;
			}

			case T_BUFFER:
			{
				g_error("Cannot handle buffers");
				return 1;
			}
		}
	}
	
	void * ret = NULL;
	switch (sig[0])
	{
		#define __ret_type(t1, t2) \
			case t1: \
				ret = g_malloc(sizeof(t2)); \
				break;

		__ret_type(T_BOOLEAN, boolean)
		__ret_type(T_INTEGER, int)
		__ret_type(T_DOUBLE, double)
		__ret_type(T_CHAR, char)
		__ret_type(T_STRING, char *)

		case T_ARRAY_BOOLEAN:
		case T_ARRAY_INTEGER:
		case T_ARRAY_DOUBLE:
			g_error("Cannot handle arrays");
			return 1;

		case T_BUFFER:
			g_error("Cannot handle buffers");
			return 1;
	}

	if (max_asyscall(hand, argv[1], sig, ret, args->pdata) != 0)
	{
		g_error("Error: %s", max_error(hand));
		return 1;
	}


	switch (method_returntype(sig))
	{
		case T_BOOLEAN:
		{
			if (*(boolean *)ret)
				g_print("true\n");
			else
				g_print("false\n");
			break;
		}

		case T_INTEGER:
			g_print("%d\n", *(int *)ret);
			break;

		case T_DOUBLE:
			g_print("%f\n", *(double *)ret);
			break;

		case T_CHAR:
			g_print("%c\n", *(char *)ret);
			break;

		case T_STRING:
			g_print("%s\n", *(char **)ret);
			break;

		default:
			g_print("Done\n");
			break;
	}

	return 0;
}
