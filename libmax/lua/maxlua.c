#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <max.h>


static GHashTable * parsesyscalls(gchar * str)
{
	GHashTable * syscalls = g_hash_table_new(g_str_hash, g_str_equal);

	GError * err = NULL;
	GRegex * regex = g_regex_new("^([[:alpha:]_]+)\\(([:vildcsbILD]+)\\)$", G_REGEX_OPTIMIZE, 0, &err);
	if (regex == NULL)
	{
		g_warning("Regex failed to compile: %s", err->message);
		g_error_free(err);
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

const gchar * type(gchar t)
{
	switch (t)
	{
		case T_VOID:			return "void";
		case T_BOOLEAN:			return "boolean";
		case T_INTEGER:			return "integer";
		case T_DOUBLE:			return "number";
		case T_CHAR:			return "character";
		case T_STRING:			return "string";
		case T_ARRAY_BOOLEAN:	return "boolean array";
		case T_ARRAY_INTEGER:	return "integer array";
		case T_ARRAY_DOUBLE:	return "number array";
		case T_BUFFER:			return "buffer";
		default:				return "unknown type";
	}
}

static int l_syscall(lua_State * L)
{
	const gchar * name = lua_tostring(L, lua_upvalueindex(1));
	const gchar * sig = lua_tostring(L, lua_upvalueindex(2));
	maxhandle_t * handle = lua_touserdata(L, lua_upvalueindex(3));

	//get return type
	gboolean free_ret;
	gpointer ret;
	switch (sig[0])
	{
		#define __syscall_elem1(t1, t2, b) \
			case t1: \
				ret = g_malloc0(sizeof(t2)); \
				free_ret = b; \
				break;

		__syscall_elem1(T_BOOLEAN, gboolean, FALSE)
		__syscall_elem1(T_INTEGER, gint, FALSE)
		__syscall_elem1(T_DOUBLE, gdouble, FALSE)
		__syscall_elem1(T_CHAR, gchar, FALSE)
		__syscall_elem1(T_STRING, gchar *, TRUE)

		case T_VOID:
		case ':':
			ret = NULL;
			free_ret = FALSE;
			break;

		case T_ARRAY_BOOLEAN:
		case T_ARRAY_INTEGER:
		case T_ARRAY_DOUBLE:
		case T_BUFFER:
			g_warning("Cannot handle syscalls with buffer/arrays (yet)");
			return 0;

		default:
			g_warning("Unknown syscall return type %c", sig[0]);
			return 0;
	}

	GPtrArray * args = g_ptr_array_new();
	gchar * s = (gchar *)sig;
	gint i=1;
	while (*s++ != ':' && *s != '\0') {} //go past first ':'

	while (*s != '\0')
	{
		if (lua_isnone(L, i) && *s != T_VOID)
		{
			luaL_error(L, "Invalid argument. Argument %d must be a %s", i, type(*s));
		}

		switch (*s)
		{
			#define __syscall_elem2(t1, t2, f) \
				case t1: { \
					gpointer v = g_malloc0(sizeof(t2)); \
					*(t2 *)v = f; \
					g_ptr_array_add(args, v); \
					break; }

			__syscall_elem2(T_BOOLEAN, gboolean, lua_toboolean(L, i))
			__syscall_elem2(T_INTEGER, gint, lua_tointeger(L, i))
			__syscall_elem2(T_DOUBLE, gdouble, (gdouble)lua_tonumber(L, i))
			__syscall_elem2(T_CHAR, gchar, (gchar)lua_tointeger(L, i))
			__syscall_elem2(T_STRING, gchar *, (gchar *)lua_tostring(L, i))

			case T_VOID:
				break;

			case T_ARRAY_BOOLEAN:
			case T_ARRAY_INTEGER:
			case T_ARRAY_DOUBLE:
			case T_BUFFER:
				g_warning("Cannot handle syscalls with buffer/arrays (yet)");
				return 0;

			default:
				g_warning("Unknown syscall parameter type %c", *s);
				return 0;

		}

		i++;
		s++;
	}

	if (max_asyscall(handle, name, sig, ret, args->pdata) == -1)
	{
		g_warning("Syscall %s failed to execute: %s", name, max_error(handle));
		lua_pushnil(L);
	}
	else
	{
		switch (sig[0])
		{
			#define __syscall_elem3(t1, t2, f) \
				case t1: \
					f(L, *(t2 *)ret); \
					break;

			__syscall_elem3(T_BOOLEAN, gboolean, lua_pushboolean)
			__syscall_elem3(T_INTEGER, gint, lua_pushinteger)
			__syscall_elem3(T_DOUBLE, gdouble, lua_pushnumber)
			__syscall_elem3(T_STRING, gchar *, lua_pushstring)

			case T_CHAR:
			{
				gchar c[2] = { *(gchar *)ret, '\0' };
				lua_pushstring(L, c);
				break;
			}

			case T_VOID:
			case ':':
				lua_pushnil(L);
				break;
		}
	}

	//free return vars
	if (free_ret)
		g_free(*(gchar **)ret);
	g_free(ret);

	//free args
	i=0;
	for (; i<args->len; i++)
		g_free(args->pdata[i]);
	g_ptr_array_free(args, TRUE);

	return 1;
}

static int l_connect(lua_State * L)
{
	maxhandle_t * handle;

	if (lua_isnone(L, 1))
	{
		handle = max_local();
	}
	else
	{
		const gchar * ip = lua_tostring(L, 1);
		handle = max_remote(ip);
	}

	if (max_iserror(handle))
	{
		g_warning("Could not connect to max: %s", max_error(handle));
		lua_pushnil(L);
		return 1;
	}

	//get list of syscalls
	char * syscalls_str = NULL;
	if (max_syscall(handle, "syscall_list", "s:v", &syscalls_str) != 0)
	{
		g_warning("Could not call kernel max_syscalls function");
		max_close(handle);

		lua_pushnil(L);
		return 1;
	}

	GHashTable * syscalls = parsesyscalls(syscalls_str);
	if (syscalls == NULL)
	{
		max_close(handle);

		lua_pushnil(L);
		return 1;
	}

	free(syscalls_str);

	//now register all syscalls
	GHashTableIter itr;
	gchar * name, * sig;
	g_hash_table_iter_init(&itr, syscalls);

	lua_newtable(L);
	while (g_hash_table_iter_next(&itr, (gpointer *)&name, (gpointer *)&sig))
	{
		lua_pushstring(L, name);
		lua_pushstring(L, name);
		lua_pushstring(L, sig);
		lua_pushlightuserdata(L, handle);
		lua_pushcclosure(L, l_syscall, 3);
		lua_settable(L, -3);
	}

	g_hash_table_destroy(syscalls);

	//register handle
	lua_pushstring(L, "__handle");
	lua_pushlightuserdata(L, handle);
	lua_settable(L, -3);

	return 1;
}

static int l_close(lua_State * L)
{
	lua_getfield(L, 1, "__handle");
	maxhandle_t * handle = lua_touserdata(L, -1);

	max_close(handle);

	return 0;
}


static const luaL_reg maxlib[] = {
		{"connect", l_connect},
		{"close", l_close},
		{NULL, NULL}
};

LUALIB_API int luaopen_max(lua_State * L)
{
	luaL_register(L, "max", maxlib);
	return 1;
}
