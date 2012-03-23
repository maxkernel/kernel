#ifdef USE_LUA

#include <string.h>
#include <errno.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <maxmodel/interpret.h>


#define ENV_METATABLE		"MaxModel.env"
#define ENTRY_METATABLE		"MaxModel.entry"

typedef struct
{
	const char * path;
	const interpret_callbacks * cbs;
	model_t * model;
	model_script_t * script;
} luaenv_t;

typedef modelhead_t * entry_t;

static int l_dolog(lua_State * L, level_t level)
{
	luaL_argcheck(L, lua_type(L, 1) == LUA_TSTRING, 1, "must be string");

	luaenv_t * env = luaL_checkudata(L, lua_upvalueindex(1), ENV_METATABLE);
	if (env->cbs->log == NULL)
	{
		return 0;
	}

	env->cbs->log(level, lua_tostring(L, 1));
	return 0;
}

static int l_debug(lua_State * L)	{	return l_dolog(L, LEVEL_DEBUG);		}
static int l_log(lua_State * L)		{	return l_dolog(L, LEVEL_INFO);		}
static int l_print(lua_State * L)	{	return l_dolog(L, LEVEL_INFO);		}
static int l_warn(lua_State * L)	{	return l_dolog(L, LEVEL_WARNING);	}
static int l_error(lua_State * L)
{
	luaL_argcheck(L, lua_type(L, 1) == LUA_TSTRING, 1, "must be string");
	return luaL_error(L, "script: %s", lua_tostring(L, 1));
}

static int l_loadmodule(lua_State * L)
{
	luaL_argcheck(L, lua_type(L, 1) == LUA_TSTRING, 1, "must be string");
	const char * modulename = luaL_checkstring(L, 1);

	exception_t * e = NULL;
	entry_t * entry = NULL;

	luaenv_t * env = luaL_checkudata(L, lua_upvalueindex(1), ENV_METATABLE);
	meta_t * meta = env->cbs->metalookup(modulename, &e);
	{
		if (meta == NULL || exception_check(&e))
		{
			return luaL_error(L, "loadmodule failed (meta): %s", (e == NULL)? "Unknown error!" : e->message);
		}

		model_module_t * module = model_module_new(env->model, env->script, meta, &e);
		if (module == NULL || exception_check(&e))
		{
			return luaL_error(L, "loadmodule failed (module): %s", (e == NULL)? "Unknown error!" : e->message);
		}

		entry = lua_newuserdata(L, sizeof(entry_t));
		luaL_getmetatable(L, ENTRY_METATABLE);
		lua_setmetatable(L, -2);

		*entry = &module->head;
	}
	meta_destroy(meta);

	return 1;
}

static int l_newblockinst(lua_State * L)
{

	return 0;
}


static int mt_entry_index(lua_State * L)
{
	luaL_argcheck(L, lua_type(L, 1) == LUA_TUSERDATA, 1, "must be userdata");
	luaL_argcheck(L, lua_type(L, 2) == LUA_TSTRING, 2, "must be string");
	entry_t * entry = luaL_checkudata(L, 1, ENTRY_METATABLE);
	const char * key = luaL_checkstring(L, 2);

	//luaenv_t * env = luaL_checkudata(L, lua_upvalueindex(1), ENV_METATABLE);
	switch ((*entry)->type)
	{
		case model_module:
		{
			if (strcmp(key, "config") == 0)
			{
				// We are a config value, just return self
				lua_pushvalue(L, 1);
				return 1;
			}
			else
			{
				// We are a block reference, push a closure to create the new instance on the stack
				lua_pushvalue(L, lua_upvalueindex(1));
				lua_pushvalue(L, 1);
				lua_pushvalue(L, 2);
				lua_pushcclosure(L, l_newblockinst, 3);
				return 1;
			}
		}

		default:
		{
			return luaL_error(L, "Unknown userdata entry: %X", (*entry)->type);
		}
	}

	return 1;
}

static int mt_entry_newindex(lua_State * L)
{
	luaL_argcheck(L, lua_type(L, 1) == LUA_TUSERDATA, 1, "must be userdata");
	luaL_argcheck(L, lua_type(L, 2) == LUA_TSTRING, 2, "must be string");
	luaL_argcheck(L, lua_type(L, 3) == LUA_TBOOLEAN || lua_type(L, 3) == LUA_TNUMBER || lua_type(L, 3) == LUA_TSTRING, 3, "must be boolean, number, or string");
	entry_t * entry = luaL_checkudata(L, 1, ENTRY_METATABLE);
	const char * key = luaL_checkstring(L, 2);
	const char * value = luaL_checkstring(L, 3);

	exception_t * e = NULL;

	luaenv_t * env = luaL_checkudata(L, lua_upvalueindex(1), ENV_METATABLE);
	switch ((*entry)->type)
	{
		case model_module:
		{
			//modelhead_t * head = *entry;
			model_configparam_t * configparam = model_configparam_new(env->model, (model_module_t *)(*entry), key, value, &e);
			if (configparam == NULL || exception_check(&e))
			{
				return luaL_error(L, "configparam failed: %s", (e == NULL)? "Unknown error!" : e->message);
			}

			break;
		}

		default:
		{
			return luaL_error(L, "Unknown userdata entry: %d", (*entry)->type);
		}
	}

	return 0;
}

bool interpret_lua(model_t * model, const char * path, const interpret_callbacks * cbs, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return false;
		}

		if (model == NULL || path == NULL || cbs == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}

		if (cbs->metalookup == NULL)
		{
			exception_set(err, EINVAL, "metalookup must be set in callbacks!");
			return false;
		}
	}

	bool ret = true;
	luaenv_t env = {
		.path = path,
		.cbs = cbs,
		.model = model,
		.script = model_script_new(model, path, err),
	};

	if (exception_check(err) || env.script == NULL)
	{
		return false;
	}


	lua_State * L = luaL_newstate();
	luaL_openlibs(L);

	// TODO - add lua_atpanic

	// Create the metatables
	lua_pushlightuserdata(L, &env);
	#define metatable_register(name, func) \
		(lua_pushstring(L, name), lua_pushvalue(L, -3), lua_pushcclosure(L, func, 1), lua_settable(L, -3))
	{
		luaL_newmetatable(L, ENV_METATABLE);
		lua_setmetatable(L, -2);

		luaL_newmetatable(L, ENTRY_METATABLE);
		metatable_register("__index", mt_entry_index);
		metatable_register("__newindex", mt_entry_newindex);
		lua_pop(L, 1);
	}


	// Register the global functions
	#define function_register(func)	\
		(lua_pushvalue(L, -1), lua_pushcclosure(L, l_ ## func, 1), lua_setglobal(L, (#func)))
	{
		function_register(debug);
		function_register(log);
		function_register(print);
		function_register(warn);
		function_register(error);

		//interpreter_register(setpath);
		//interpreter_register(appendpath);

		function_register(loadmodule);
	}
	lua_pop(L, 1);

	// Run the script
	if (luaL_loadfile(L, path) || lua_pcall(L, 0,0,0))
	{
		exception_set(err, ECANCELED, "Lua error: %s", lua_tostring(L, -1));
		ret = false;
	}
	/*
	// Register the functions
	lua_register(L, "log", l_log);
	lua_register(L, "print", l_log);
	lua_register(L, "warn", l_warn);
	lua_register(L, "debug", l_debug);
	lua_register(L, "setpath", l_setpath);
	lua_register(L, "appendpath", l_appendpath);
	//lua_register(L, "include", l_include);	// TODO - come up with an alternative to this!
	//lua_register(L, "dofile", l_include);
	lua_register(L, "loadmodule", l_loadmodule);
	lua_register(L, "route", l_route);
	lua_register(L, "newrategroup", l_newrategroup);
	lua_register(L, "newsyscall", l_newsyscall);

	LUALOG(LOG_INFO, "Executing lua file %s", path);
	if (luaL_loadfile(L, path) || lua_pcall(L, 0,0,0))
	{
		LUALOG(LOG_ERR, "Lua: %s", lua_tostring(L, -1));
		ret = false;
	}

	lua_close(L);
	return ret;
	*/

	// Close down
	lua_close(L);
	return ret;
}

#endif
