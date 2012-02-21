#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <serialize.h>
#include <max.h>


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
