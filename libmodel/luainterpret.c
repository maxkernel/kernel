#ifdef USE_LUA

#include <string.h>
#include <errno.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <maxmodel/interpret.h>


#define ENV_METATABLE		"MaxModel.env"
#define ENTRY_METATABLE		"MaxModel.entry"

#define CONFIG_FLAG			"config"

typedef struct
{
	const char * path;
	const interpret_callbacks * cbs;
	model_t * model;
	model_script_t * script;
} luaenv_t;

typedef struct
{
	modelhead_t * head;
	char name[MODEL_SIZE_MAX];
} entry_t;

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

static int l_dopath(lua_State * L, bool (*pathfunc)(const char *, exception_t **))
{
	luaL_argcheck(L, lua_type(L, 1) == LUA_TSTRING, 1, "must be string");

	if (pathfunc == NULL)
	{
		return 0;
	}

	exception_t * e = NULL;
	if (!pathfunc(lua_tostring(L, 1), &e) || exception_check(&e))
	{
		return luaL_error(L, "config path failed: %s", (e == NULL)? "Unknown error!" : e->message);
	}

	return 0;
}

static int l_setpath(lua_State * L)
{
	luaenv_t * env = luaL_checkudata(L, lua_upvalueindex(1), ENV_METATABLE);
	return l_dopath(L, env->cbs->setpath);
}

static int l_appendpath(lua_State * L)
{
	luaenv_t * env = luaL_checkudata(L, lua_upvalueindex(1), ENV_METATABLE);
	return l_dopath(L, env->cbs->appendpath);
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

		model_module_t * module = model_newmodule(env->model, env->script, meta, &e);
		if (module == NULL || exception_check(&e))
		{
			return luaL_error(L, "loadmodule failed (module): %s", (e == NULL)? "Unknown error!" : e->message);
		}

		entry = lua_newuserdata(L, sizeof(entry_t));
		memset(entry, 0, sizeof(entry_t));
		luaL_getmetatable(L, ENTRY_METATABLE);
		lua_setmetatable(L, -2);

		entry->head = &module->head;
	}
	meta_destroy(meta);

	return 1;
}

static int l_newblockinst(lua_State * L)
{
	luaenv_t * env = luaL_checkudata(L, lua_upvalueindex(1), ENV_METATABLE);
	entry_t * entry = luaL_checkudata(L, lua_upvalueindex(2), ENTRY_METATABLE);
	const char * blockname = luaL_checkstring(L, lua_upvalueindex(3));

	modelhead_t * head = entry->head;
	if (head->type != model_module)
	{
		return luaL_error(L, "Not a module! (Could not create block instance)");
	}

	model_module_t * module = (model_module_t *)head;
	meta_t * backing = module->backing;

	const meta_block_t * block = NULL;
	if (!meta_lookupblock(backing, blockname, &block))
	{
		const char * path = NULL;
		meta_getinfo(backing, &path, NULL, NULL, NULL, NULL);
		return luaL_error(L, "Could not find block '%s' in module %s.", blockname, path);
	}

	const char * sig = NULL;
	meta_getblock(block, NULL, NULL, NULL, &sig, NULL, NULL);

	size_t args_length = strlen(sig);
	const char * args[args_length];
	memset(args, 0, sizeof(const char *) * args_length);

	for (size_t i = 0; i < args_length; i++)
	{
		if (lua_type(L, i+1) == LUA_TBOOLEAN)
		{
			// Set that item on the stack to an integer
			bool v = lua_toboolean(L, i+1);
			lua_pushstring(L, (v)? "true" : "false");
			lua_replace(L, i+1);
		}

		args[i] = luaL_checkstring(L, i+1);
	}

	exception_t * e = NULL;
	model_linkable_t * blockinst = model_newblockinst(env->model, module, env->script, blockname, args, args_length, &e);
	if (blockinst == NULL || exception_check(&e))
	{
		return luaL_error(L, "blockinst failed: %s", (e == NULL)? "Unknown error!" : e->message);
	}

	entry_t * newentry = lua_newuserdata(L, sizeof(entry_t));
	memset(newentry, 0, sizeof(entry_t));
	luaL_getmetatable(L, ENTRY_METATABLE);
	lua_setmetatable(L, -2);

	newentry->head = &blockinst->head;

	return 1;
}

static int l_route(lua_State * L)
{
	luaL_argcheck(L, lua_type(L, 1) == LUA_TUSERDATA, 1, "must be userdata");
	luaL_argcheck(L, lua_type(L, 2) == LUA_TUSERDATA, 2, "must be userdata");

	luaenv_t * env = luaL_checkudata(L, lua_upvalueindex(1), ENV_METATABLE);
	entry_t * out = luaL_checkudata(L, 1, ENTRY_METATABLE);
	entry_t * in = luaL_checkudata(L, 2, ENTRY_METATABLE);

	if (!model_linkable(out->head->type))
	{
		return luaL_error(L, "Output not a valid linkable!");
	}

	if (!model_linkable(in->head->type))
	{
		return luaL_error(L, "Input not a valid linkable!");
	}

	exception_t * e = NULL;
	model_link_t * link = model_newlink(env->model, env->script, (model_linkable_t *)out->head, out->name, (model_linkable_t *)in->head, in->name, &e);
	if (link == NULL || exception_check(&e))
	{
		return luaL_error(L, "link failed: %s", (e == NULL)? "Unknown error!" : e->message);
	}

	return 0;
}

static int l_newrategroup(lua_State * L)
{
	luaL_argcheck(L, lua_type(L, 1) == LUA_TSTRING, 1, "must be string");
	luaL_argcheck(L, lua_type(L, 2) == LUA_TTABLE, 2, "must be table");
	luaL_argcheck(L, lua_type(L, 3) == LUA_TNUMBER, 3, "must be number");

	luaenv_t * env = luaL_checkudata(L, lua_upvalueindex(1), ENV_METATABLE);
	const char * name = luaL_checkstring(L, 1);
	double rate_hz = luaL_checknumber(L, 3);

	size_t index = 0;
	const model_linkable_t * blockinsts[MODEL_MAX_RATEGROUPELEMS] = { NULL };

	lua_pushnil(L);
	while (lua_next(L, 2))
	{
		if (index >= MODEL_MAX_RATEGROUPELEMS)
		{
			return luaL_error(L, "Too many rategroup elements! (MODEL_MAX_RATEGROUPELEMS = %d)", MODEL_MAX_RATEGROUPELEMS);
		}

		entry_t * entry = luaL_checkudata(L, -1, ENTRY_METATABLE);
		if (entry->head->type != model_blockinst || strcmp(entry->name, "") != 0)
		{
			return luaL_error(L, "Invalid blockinst for item #%d", index+1);
		}

		blockinsts[index] = (model_linkable_t *)entry->head;

		lua_pop(L, 1);
		index += 1;
	}

	exception_t * e = NULL;
	model_linkable_t * rg = model_newrategroup(env->model, env->script, name, rate_hz, blockinsts, index, &e);
	if (rg == NULL || exception_check(&e))
	{
		return luaL_error(L, "rategroup failed: %s", (e == NULL)? "Unknown error!" : e->message);
	}

	entry_t * entry = lua_newuserdata(L, sizeof(entry_t));
	memset(entry, 0, sizeof(entry_t));
	luaL_getmetatable(L, ENTRY_METATABLE);
	lua_setmetatable(L, -2);

	entry->head = &rg->head;

	return 1;
}

static int l_newsyscall(lua_State * L)
{
	luaL_argcheck(L, lua_type(L, 1) == LUA_TSTRING, 1, "must be string (syscall name)");
	luaL_argcheck(L, lua_type(L, 2) == LUA_TSTRING, 2, "must be string (syscall signature)");
	luaL_argcheck(L, lua_type(L, 3) == LUA_TTABLE, 3, "must be table (parameters)");
	luaL_argcheck(L, lua_type(L, 4) == LUA_TUSERDATA || lua_type(L, 4) == LUA_TNIL, 4, "must be userdata or nil (return)");
	luaL_argcheck(L, lua_type(L, 5) == LUA_TSTRING, 5, "must be string (documentation string)");

	luaenv_t * env = luaL_checkudata(L, lua_upvalueindex(1), ENV_METATABLE);
	const char * name = luaL_checkstring(L, 1);
	const char * sig = luaL_checkstring(L, 2);
	entry_t * retvalue = NULL;
	const char * desc = luaL_checkstring(L, 5);

	if (!lua_isnil(L, 4))
	{
		retvalue = luaL_checkudata(L, 3, ENTRY_METATABLE);
		if (!model_linkable(retvalue->head->type))
		{
			return luaL_error(L, "Invalid linkable for item #r");
		}
	}

	exception_t * e = NULL;
	model_linkable_t * syscall = model_newsyscall(env->model, env->script, name, sig, desc, &e);
	if (syscall == NULL || exception_check(&e))
	{
		return luaL_error(L, "syscall failed: %s", (e == NULL)? "Unknown error!" : e->message);
	}

	entry_t * entry = lua_newuserdata(L, sizeof(entry_t));
	memset(entry, 0, sizeof(entry_t));
	luaL_getmetatable(L, ENTRY_METATABLE);
	lua_setmetatable(L, -2);

	entry->head = &syscall->head;

	// Link the return value
	if (retvalue != NULL)
	{
		model_link_t * link = model_newlink(env->model, env->script, (model_linkable_t *)retvalue->head, retvalue->name, syscall, "r", &e);
		if (link == NULL || exception_check(&e))
		{
			return luaL_error(L, "link(r) failed: %s", (e == NULL)? "Unknown error!" : e->message);
		}
	}

	size_t index = 0;

	// Link all the argument values
	lua_pushnil(L);
	while (lua_next(L, 3))
	{
		if (!lua_isnil(L, -1))
		{
			entry_t * avalue = luaL_checkudata(L, -1, ENTRY_METATABLE);
			if (!model_linkable(avalue->head->type))
			{
				return luaL_error(L, "Invalid linkable for item #a%d", index+1);
			}

			string_t aname = string_new("a%d", index+1);

			model_link_t * link = model_newlink(env->model, env->script, syscall, aname.string, (model_linkable_t *)avalue->head, avalue->name, &e);
			if (link == NULL || exception_check(&e))
			{
				return luaL_error(L, "link(a%d) failed: %s", index+1, (e == NULL)? "Unknown error!" : e->message);
			}
		}

		lua_pop(L, 1);
		index += 1;
	}

	return 1;
}


static int mt_entry_index(lua_State * L)
{
	luaL_argcheck(L, lua_type(L, 1) == LUA_TUSERDATA, 1, "must be userdata");
	luaL_argcheck(L, lua_type(L, 2) == LUA_TSTRING || lua_type(L, 2) == LUA_TNUMBER, 2, "must be string");

	//luaenv_t * env = luaL_checkudata(L, lua_upvalueindex(1), ENV_METATABLE);
	entry_t * entry = luaL_checkudata(L, 1, ENTRY_METATABLE);
	const char * key = luaL_checkstring(L, 2);

	if (strlen(key) >= MODEL_SIZE_MAX)
	{
		return luaL_error(L, "Key label too long! (MODEL_SIZE_MAX = %d) '%s'", MODEL_SIZE_MAX, key);
	}

	switch (entry->head->type)
	{
		case model_module:
		{
			if (strcmp(key, CONFIG_FLAG) == 0)
			{
				entry_t * newentry = lua_newuserdata(L, sizeof(entry_t));
				memset(newentry, 0, sizeof(entry_t));
				luaL_getmetatable(L, ENTRY_METATABLE);
				lua_setmetatable(L, -2);

				newentry->head = entry->head;
				strcpy(newentry->name, CONFIG_FLAG);

				return 1;
			}
			else if (meta_lookupblock(((model_module_t *)entry->head)->backing, key, NULL))
			{
				// We are a block reference, push a closure to create the new instance on the stack
				lua_pushvalue(L, lua_upvalueindex(1));
				lua_pushvalue(L, 1);
				lua_pushvalue(L, 2);
				lua_pushcclosure(L, l_newblockinst, 3);
				return 1;
			}
			else
			{
				meta_t * backing = ((model_module_t *)entry->head)->backing;

				const char * path = NULL;
				meta_getinfo(backing, &path, NULL, NULL, NULL, NULL);
				return luaL_error(L, "Unknown module field '%s' in module %s", key, path);
			}
		}

		case model_blockinst:
		case model_rategroup:
		case model_syscall:
		{
			entry_t * newentry = lua_newuserdata(L, sizeof(entry_t));
			memset(newentry, 0, sizeof(entry_t));
			luaL_getmetatable(L, ENTRY_METATABLE);
			lua_setmetatable(L, -2);

			newentry->head = entry->head;
			if (strlen(entry->name) == 0)
			{
				// New entry, ex: pin1
				strcpy(newentry->name, key);
			}
			else
			{
				// Concatenating to an entry, ex: pin1[2]
				if ((strlen(entry->name) + strlen(key) + strlen("[]")) >= MODEL_SIZE_MAX)
				{
					return luaL_error(L, "Key label too long! (MODEL_SIZE_MAX = %d) %s[%s]", MODEL_SIZE_MAX, entry->name, key);
				}

				sprintf(newentry->name, "%s[%s]", entry->name, key);
			}


			return 1;
		}

		default:
		{
			return luaL_error(L, "Unknown userdata entry: %d", entry->head->type);
		}
	}

	return 0;
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
	switch (entry->head->type)
	{
		case model_module:
		{
			if (strcmp(entry->name, CONFIG_FLAG) == 0)
			{
				model_config_t * configparam = model_newconfig(env->model, (model_module_t *)entry->head, key, value, &e);
				if (configparam == NULL || exception_check(&e))
				{
					return luaL_error(L, "configparam failed: %s", (e == NULL)? "Unknown error!" : e->message);
				}
			}
			else
			{
				meta_t * backing = ((model_module_t *)entry->head)->backing;

				const char * path = NULL;
				meta_getinfo(backing, &path, NULL, NULL, NULL, NULL);
				return luaL_error(L, "Unknown module field '%s' in module %s", key, path);
			}
			break;
		}

		default:
		{
			return luaL_error(L, "Unknown userdata entry: %d", entry->head->type);
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
		.script = model_newscript(model, path, err),
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

		function_register(setpath);
		function_register(appendpath);

		function_register(loadmodule);
		function_register(route);
		function_register(newrategroup);
		function_register(newsyscall);
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

	//lua_register(L, "include", l_include);	// TODO - come up with an alternative to this!
	//lua_register(L, "dofile", l_include);
	*/

	// Close down
	lua_close(L);
	return ret;
}

#endif
