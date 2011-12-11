#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <serialize.h>
#include <max.h>

#if 0
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
#endif

/*
const char * type(char t)
{
	switch (t)
	{
		case T_VOID:			return "void";
		case T_BOOLEAN:			return "boolean";
		case T_INTEGER:			return "integer";
		case T_DOUBLE:			return "number";
		case T_CHAR:			return "character";
		case T_STRING:			return "string";
		default:				return "unknown type";
	}
}
*/

#if 0
static int l_syscall(lua_State * L)
{
	const gchar * name = lua_tostring(L, lua_upvalueindex(1));
	const gchar * sig = lua_tostring(L, lua_upvalueindex(2));
	maxhandle_t * handle = lua_touserdata(L, lua_upvalueindex(3));

	//get return type
	bool free_ret;
	gpointer ret;
	switch (sig[0])
	{
		#define __syscall_elem1(t1, t2, b) \
			case t1: \
				ret = g_malloc0(sizeof(t2)); \
				free_ret = b; \
				break;

		__syscall_elem1(T_BOOLEAN, bool, false)
		__syscall_elem1(T_INTEGER, gint, false)
		__syscall_elem1(T_DOUBLE, gdouble, false)
		__syscall_elem1(T_CHAR, gchar, false)
		__syscall_elem1(T_STRING, gchar *, true)

		case T_VOID:
		case ':':
			ret = NULL;
			free_ret = false;
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

			__syscall_elem2(T_BOOLEAN, bool, lua_toboolean(L, i))
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

			__syscall_elem3(T_BOOLEAN, bool, lua_pushboolean)
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
	g_ptr_array_free(args, true);

	return 1;
}
#endif

static int l_dosyscall(lua_State * L)
{
	maxhandle_t * hand = lua_touserdata(L, lua_upvalueindex(1));
	const char * name = lua_tostring(L, lua_upvalueindex(2));

	// Try to get the syscall from the cache
	exception_t * err = NULL;
	syscall_t * syscall = max_syscallcache_lookup(hand, &err, name);

	if (exception_check(&err) || syscall == NULL)
	{
		string_t msg = string_new("Unable to find syscall %s: %s", name, err == NULL? "Syscall doesn't exist" : err->message);
		exception_free(err);
		return luaL_error(L, "%s", msg.string);
	}

	int processed = 0;

	ssize_t copyarg(void * array, size_t arraylen, exception_t ** e, const char * sig, int index)
	{
		ssize_t copy(const void * ptr, size_t s)
		{
			if (s > arraylen)
			{
				return -1;
			}

			memcpy(array, ptr, s);
			return s;
		}

		int luaindex = index + 1;
		processed += 1;

		switch (sig[index])
		{
			case T_BOOLEAN:
			{
				luaL_argcheck(L, lua_isboolean(L, luaindex), luaindex, "must be boolean");
				bool v = lua_toboolean(L, luaindex);
				return copy(&v, sizeof(bool));
			}

			case T_INTEGER:
			{
				luaL_argcheck(L, lua_isnumber(L, luaindex), luaindex, "must be an integer");
				int v = lua_tointeger(L, luaindex);
				return copy(&v, sizeof(int));
			}

			case T_DOUBLE:
			{
				luaL_argcheck(L, lua_isnumber(L, luaindex), luaindex, "must be double");
				double v = lua_tonumber(L, luaindex);
				return copy(&v, sizeof(double));
			}

			case T_CHAR:
			{
				luaL_argcheck(L, lua_isstring(L, luaindex), luaindex, "must be char");
				const char v = lua_tostring(L, luaindex)[0];
				return copy(&v, sizeof(char));
			}

			case T_STRING:
			{
				luaL_argcheck(L, lua_isstring(L, luaindex), luaindex, "must be string");
				const char * v = lua_tostring(L, luaindex);
				return copy(v, strlen(v) + 1);
			}
		}

		// Should never get here
		return -1;
	}


	const char * sig = method_params(syscall->sig);
	char buffer[max_getheadersize(hand) + max_getbuffersize(hand)];
	void ** header = (void **)buffer;

	ssize_t wrote = serialize_2array_fromcb_wheader(header, sizeof(buffer), sig, copyarg);
	if (wrote < 0)
	{
		return luaL_error(L, "Could not serialize syscall %s parameters: %s", syscall->name, strerror(errno));
	}

	if (lua_gettop(L) != processed)
	{
		// Some parameters not processed
		return luaL_error(L, "Extra arguments sent to syscall. Expected signature %s", sig);
	}

	return_t ret;
	bool success = max_asyscall(hand, &err, &ret, syscall->name, syscall->sig, header);
	if (exception_check(&err) || !success)
	{
		string_t msg = string_new("Could not complete syscall %s: %s", syscall->name, err == NULL? "Unknown Error" : err->message);
		exception_free(err);
		return luaL_error(L, "%s", msg.string);
	}

	switch (ret.sig)
	{
		case T_VOID:		lua_pushnil(L);								break;
		case T_BOOLEAN:		lua_pushboolean(L, ret.data.t_boolean);		break;
		case T_INTEGER:		lua_pushinteger(L, ret.data.t_integer);		break;
		case T_DOUBLE:		lua_pushnumber(L, ret.data.t_double);		break;
		case T_CHAR:		lua_pushlstring(L, &ret.data.t_char, 1);	break;
		case T_STRING:		lua_pushstring(L, ret.data.t_string);		break;
	}

	return 1;
}

static int mt_handle_index(lua_State * L)
{
	maxhandle_t * hand = luaL_checkudata(L, 1, "MaxLib.handle");
	const char * key = luaL_checkstring(L, 2);

	lua_pushlightuserdata(L, hand);
	lua_pushstring(L, key);
	lua_pushcclosure(L, l_dosyscall, 2);

	return 1;
}

static int mt_handle_gc(lua_State * L)
{
	maxhandle_t * handle = luaL_checkudata(L, 1, "MaxLib.handle");
	max_destroy(handle);

	return 0;
}

static int l_connect(lua_State * L)
{
	const char * host = NULL;
	if (!lua_isnoneornil(L, 1))
	{
		host = lua_tostring(L, 1);
	}

	maxhandle_t * handle = lua_newuserdata(L, sizeof(maxhandle_t));
	max_initialize(handle);
	max_syscallcache_enable(handle);

	luaL_getmetatable(L, "MaxLib.handle");
	lua_setmetatable(L, -2);

	exception_t * err = NULL;
	bool success = false;

	if (host == NULL)
	{
		success = max_connectlocal(handle, &err);
	}
	else
	{
		success = max_connect(handle, &err, host);
	}

	if (exception_check(&err) || !success)
	{
		string_t msg = string_new("Could not connect to max: %s", err == NULL? "Unknown Error" : err->message);
		exception_free(err);
		return luaL_error(L, "%s", msg.string);
	}

	return 1;

#if 0
	//get list of syscalls
	char * syscalls_str = NULL;
	if (max_syscall(handle, "syscall_list", "s:v", &syscalls_str) != 0)
	{
		LOG_ERROR("Could not call kernel max_syscalls function");
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
#endif

#if 0
	// Register handle
	lua_newtable(L);
	lua_pushstring(L, "__handle");
	lua_pushlightuserdata(L, handle);
	lua_settable(L, -3);

	// Register metatable
	lua_newtable(L);
	lua_pushstring(L, "__index");
	lua_pushcfunction(L, l_mtindex);
	lua_settable(L, -3);
	lua_setmetatable(L, -2);
#endif
}

static int l_close(lua_State * L)
{
	maxhandle_t * handle = luaL_checkudata(L, 1, "MaxLib.handle");
	max_destroy(handle);

	return 0;
}


static const luaL_Reg maxlib[] = {
		{"connect", l_connect},
		{"close", l_close},
		{NULL, NULL}
};

static const struct luaL_Reg handle_mt[] = {
	{"__index", mt_handle_index},
	{"__gc", mt_handle_gc},
	{NULL, NULL}
};

LUALIB_API int luaopen_max(lua_State * L)
{
	luaL_newmetatable(L, "MaxLib.handle");
	luaL_register(L, NULL, handle_mt);


	luaL_register(L, "max", maxlib);
	return 1;
}
