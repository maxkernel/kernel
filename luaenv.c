#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "kernel.h"
#include "kernel-priv.h"

#define KENTRY_NAMELEN		50
#define GROUP_MAX			20

#if defined(ALPHA) || defined(BETA)
  #define LUALOG(level, format, ...) log_write(level, "LUAENV", "[%s in %s at %d]: " format, __FUNCTION__, __FILE__, __LINE__, ## __VA_ARGS__)
#else
  #define LUALOG(level, format, ...) log_write(level, "LUAENV", format, ## __VA_ARGS__)
#endif


typedef struct
{
	enum
	{
		K_MODULE		= 1,
		K_BLOCKINST		= 2,
	} type;
	char name[KENTRY_NAMELEN];
	const void * data;
} kernentry_t;


/* -------------------------- Prototypes ------------------------ */
static int mt_kernentry_index(lua_State * L);
static int mt_config_index(lua_State * L);
static int mt_config_newindex(lua_State * L);
static int l_newblock(lua_State * L);


static int mt_kernentry_index(lua_State * L)
{
	kernentry_t * entry = luaL_checkudata(L, 1, "MaxKernel.kernentry");
	const char * key = luaL_checkstring(L, 2);

	if (entry->type == K_MODULE && strcmp("new", key) == 0)
	{
		// Return a closure to the function l_newblock, the block will be instantiated there
		const module_t * module = entry->data;
		block_t * block = NULL;

		list_t * pos;
		list_foreach(pos, &module->blocks)
		{
			block_t * test = list_entry(pos, block_t, module_list);
			if (strcmp(entry->name, test->name) == 0)
			{
				// Found the block
				block = test;
				break;
			}
		}

		if (block == NULL)
		{
			return luaL_error(L, "Module %s has no member block named %s", module->path, entry->name);
		}

		lua_pushlightuserdata(L, block);
		lua_pushcclosure(L, l_newblock, 1);

		return 1;
	}
	else if (entry->type == K_BLOCKINST && strlen(entry->name) == 0)
	{
		// New block instance, push it back with desired key
		kernentry_t * newentry = lua_newuserdata(L, sizeof(kernentry_t));
		newentry->type = K_BLOCKINST;
		memcpy(newentry->name, key, strlen(key)+1);
		newentry->data = entry->data;

		luaL_getmetatable(L, "MaxKernel.kernentry");
		lua_setmetatable(L, -2);

		return 1;
	}
	else
	{
		return luaL_error(L, "%s entry '%s' has no member '%s'", (entry->type == K_MODULE)? "Module" : "Block", entry->name, key);
	}
}

static int mt_config_index(lua_State * L)
{
	kernentry_t * entry = luaL_checkudata(L, 1, "MaxKernel.config");
	const char * key = luaL_checkstring(L, 2);

	cfgentry_t * cfg = cfg_getparam(entry->name, key);
	if (cfg == NULL)
	{
		lua_pushnil(L);
	}
	else
	{
		switch (cfg->type)
		{
			case T_BOOLEAN:
				lua_pushboolean(L, *(bool *)cfg->variable);
				break;

			case T_INTEGER:
				lua_pushinteger(L, *(int *)cfg->variable);
				break;

			case T_DOUBLE:
				lua_pushnumber(L, *(double *)cfg->variable);
				break;

			case T_STRING:
				lua_pushstring(L, *(char **)cfg->variable);
				break;

			case T_CHAR:
				lua_pushlstring(L, (char *)cfg->variable, 1);
				break;
		}
	}

	return 1;
}

static int mt_config_newindex(lua_State * L)
{
	kernentry_t * entry = luaL_checkudata(L, 1, "MaxKernel.config");
	const module_t * module = entry->data;

	const char * key = luaL_checkstring(L, 2);
	const char * value = luaL_checkstring(L, 3);

	cfg_setparam(module->path, key, value);

	return 0;
}


static int l_log(lua_State * L)
{
	LUALOG(LOG_INFO, "Script: %s", lua_tostring(L, 1));
	return 0;
}

static int l_warn(lua_State * L)
{
	LUALOG(LOG_WARN, "Script: %s", lua_tostring(L, 1));
	return 0;
}

static int l_debug(lua_State * L)
{
	LUALOG(LOG_DEBUG, "Script: %s", lua_tostring(L, 1));
	return 0;
}

static int l_include(lua_State * L)
{
	// Resync the path variable
	lua_getglobal(L, "path");
	const char * newpath = lua_tostring(L, -1);
	setpath(newpath);

	// Resolve the file
	const char * file = luaL_checkstring(L, 1);
	const char * filepath = resolvepath(file, PATH_FILE);
	if (filepath == NULL)
	{
		return luaL_error(L, "Include: Could not resolve file %s", file);
	}
	else
	{
		// Load the file and the free the filepath
		int result = luaL_loadfile(L, filepath);
		FREES(filepath);

		if (result == 0)
		{
			lua_call(L, 0,0);
		}
		else
		{
			return luaL_error(L, "Lua could not execute file %s: %s", file, lua_tostring(L, -1));
		}
	}


	return 0;
}

static int l_newblock(lua_State * L)
{
	block_t * blk = lua_touserdata(L, lua_upvalueindex(1));
	const char * sig = blk->new_sig;

	void ** params = malloc0(sizeof(void *) * strlen(sig));

	// Build constructor args
	int stack = 1, index = 0;
	for (; index < strlen(sig); ++index, ++stack)
	{
		void * value = NULL;
		switch (sig[index])
		{
			case T_BOOLEAN:
			{
				value = malloc0(sizeof(bool));
				*(bool *)value = lua_toboolean(L, stack);
				break;
			}
			case T_INTEGER:
			{
				value = malloc0(sizeof(int));
				*(int *)value = luaL_checkinteger(L, stack);
				break;
			}
			case T_DOUBLE:
			{
				value = malloc0(sizeof(double));
				*(int *)value = luaL_checknumber(L, stack);
				break;
			}
			case T_STRING:
			{
				value = malloc0(sizeof(char *));
				*(const char **)value = luaL_checkstring(L, stack);
				break;
			}
			case T_CHAR:
			{
				value = malloc0(sizeof(char));
				*(char *)value = luaL_checkstring(L, stack)[0];
				break;
			}
		}

		params[index] = value;
	}

	block_inst_t * blk_inst = io_newblock(blk, params);
	if (blk_inst == NULL)
	{
		return luaL_error(L, "Could not create block instance of block %s in module %s", blk->name, blk->module->path);
	}

	for (index=0; index<strlen(sig); index++)
	{
		free(params[index]);
	}
	free(params);

	kernentry_t * entry = lua_newuserdata(L, sizeof(kernentry_t));
	entry->type = K_BLOCKINST;
	entry->name[0] = '\0';
	entry->data = blk_inst;

	luaL_getmetatable(L, "MaxKernel.kernentry");
	lua_setmetatable(L, -2);

	return 1;
}

static int mt_module_index(lua_State * L)
{
	module_t ** module = luaL_checkudata(L, 1, "MaxKernel.module");
	const char * key = luaL_checkstring(L, 2);

	kernentry_t * entry = lua_newuserdata(L, sizeof(kernentry_t));
	entry->type = K_MODULE;
	memcpy(entry->name, key, strlen(key)+1);
	entry->data = *module;

	if (strcmp("config", key) == 0)
	{
		luaL_getmetatable(L, "MaxKernel.config");
		lua_setmetatable(L, -2);
	}
	else
	{
		luaL_getmetatable(L, "MaxKernel.kernentry");
		lua_setmetatable(L, -2);
	}

	return 1;
}

static int l_loadmodule(lua_State * L)
{
	// Resync the path variable
	lua_getglobal(L, "path");
	const char * newpath = lua_tostring(L, -1);
	setpath(newpath);

	// Load the module
	const char * modname = luaL_checkstring(L, 1);
	module_t * module = module_load(modname);

	// Create the lua module object
	module_t ** modobject = lua_newuserdata(L, sizeof(module_t *));
	*modobject = module;
	luaL_getmetatable(L, "MaxKernel.module");
	lua_setmetatable(L, -2);

	return 1;
}

static int l_route(lua_State * L)
{
	kernentry_t * out = luaL_checkudata(L, 1, "MaxKernel.kernentry");
	kernentry_t * in = luaL_checkudata(L, 2, "MaxKernel.kernentry");

	const block_inst_t * get_blockinst(kernentry_t * entry)
	{
		switch (entry->type)
		{
			case K_MODULE:
			{
				const module_t * module = entry->data;

				list_t * pos;
				list_foreach(pos, &module->block_inst)
				{
					block_inst_t * inst = list_entry(pos, block_inst_t, module_list);
					if (strcmp(inst->block->name, STATIC_STR) == 0)
					{
						return inst;
					}
				}

				return NULL;
			}

			case K_BLOCKINST:
			{
				return entry->data;
			}

			default:
			{
				return NULL;
			}
		}
	}

	const block_inst_t * out_inst = get_blockinst(out);
	const block_inst_t * in_inst = get_blockinst(in);

	// Make sure in_inst and out_inst are valid
	if (out_inst == NULL || in_inst == NULL)
	{
		return luaL_error(L, "Could not route %s to %s, Invalid block instance", out->name, in->name);
	}

	// Find the input_inst and output_inst
	list_t * pos;
	boutput_inst_t * bout = NULL;
	binput_inst_t * bin = NULL;

	list_foreach(pos, &out_inst->outputs_inst)
	{
		boutput_inst_t * test = list_entry(pos, boutput_inst_t, block_inst_list);
		if (strcmp(out->name, test->output->name) == 0)
		{
			bout = test;
			break;
		}
	}

	list_foreach(pos, &in_inst->inputs_inst)
	{
		binput_inst_t * test = list_entry(pos, binput_inst_t, block_inst_list);
		if (strcmp(in->name, test->input->name) == 0)
		{
			bin = test;
			break;
		}
	}

	// Do some error checking
	if (bout == NULL)
	{
		return luaL_error(L, "Could not route %s.%s, Not an output!", out_inst->block->name, out->name);
	}
	if (bin == NULL)
	{
		return luaL_error(L, "Could not route %s.%s, Not an input!", in_inst->block->name, in->name);
	}

	// Now route it
	if (!io_route(bout, bin))
	{
		return luaL_error(L, "Failed to route IO %s.%s to %s.%s", out_inst->block->name, out->name, in_inst->block->name, in->name);
	}

	return 0;
}

static int l_newrategroup(lua_State * L)
{
	const char * name = luaL_checkstring(L, 1);
	luaL_argcheck(L, lua_type(L, 2) == LUA_TTABLE, 2, "must be a table");
	double freq_hz = luaL_checknumber(L, 3);

	// Get all the block instances
	block_inst_t ** insts = malloc0(sizeof(block_inst_t *) * (GROUP_MAX+1));
	size_t index = 0;

	luaL_getmetatable(L, "MaxKernel.module");
	luaL_getmetatable(L, "MaxKernel.kernentry");
	lua_pushnil(L);
	while (lua_next(L, 2) != 0)
	{
		if (index == GROUP_MAX)
		{
			return luaL_error(L, "Too many rategroup items (max = %d). Consider increasing value GROUP_MAX in luaenv.c", GROUP_MAX);
		}

		if (!lua_isuserdata(L, -1) || !lua_getmetatable(L, -1))
		{
			return luaL_error(L, "Encountered unknown value for item %s in rategroup table (%s)", lua_tostring(L, -3), lua_typename(L, lua_type(L, -2)));
		}

		if (lua_rawequal(L, -1, -4))
		{
			kernentry_t * entry = lua_touserdata(L, -2);
			if (entry->type != K_BLOCKINST || strlen(entry->name) != 0)
			{
				return luaL_error(L, "Invalid type for item %s in rategroup table", lua_tostring(L, -3));
			}

			insts[index++] = (block_inst_t *)entry->data;
		}
		else if (lua_rawequal(L, -1, -5))
		{
			module_t * modobject = *(module_t **)lua_touserdata(L, -2);

			bool found = false;
			list_t * pos;
			list_foreach(pos, &modobject->block_inst)
			{
				block_inst_t * inst = list_entry(pos, block_inst_t, module_list);
				if (strcmp(STATIC_STR, inst->block->name) == 0)
				{
					insts[index++] = inst;
					found = true;
					break;
				}
			}

			if (!found)
			{
				return luaL_error(L, "Invalid type for item %s in rategroup table, module does not have a static block instance", lua_tostring(L, -3));
			}
		}
		else
		{
			return luaL_error(L, "Encountered unknown userdata value for item %s in rategroup table", lua_tostring(L, -3));
		}



		lua_pop(L, 2);
	}

	// Print some debug info
	if (insts[0] != NULL)
	{
		string_t str = string_new("Creating new rategroup with update @ %f Hz and members: %s", freq_hz, insts[0]->block->name);
		size_t i = 1;
		while (insts[i] != NULL)
		{
			string_append(&str, ", %s", insts[i]->block->name);
			i++;
		}
		LUALOG(LOG_DEBUG, "%s", str.string);
	}
	else
	{
		LUALOG(LOG_WARN, "Cannot create rategroup %s with no members", name);
		free(insts);
		return 0;
	}

	// Create new rate group
	runnable_t * rungrp = exec_newrungroup(name, (runnable_t **)insts);
	trigger_t * trigger = trigger_newvarclock(name, freq_hz);

	string_t name_str = string_new("%s vrategroup", name);
	kthread_t * kth = kthread_new(string_copy(&name_str), trigger, rungrp, 0);

	kthread_schedule(kth);

	// Return the rate block_inst
	kernentry_t * entry = lua_newuserdata(L, sizeof(kernentry_t));
	entry->type = K_BLOCKINST;
	entry->name[0] = '\0';
	entry->data = trigger_varclock_getblockinst(trigger);

	luaL_getmetatable(L, "MaxKernel.kernentry");
	lua_setmetatable(L, -2);

	return 1;
}

static int l_newsyscall(lua_State * L)
{
	const char * name = luaL_checkstring(L, 1);
	luaL_argcheck(L, lua_type(L, 2) == LUA_TTABLE, 2, "must be a table");
	const char * initial_desc = lua_tostring(L, 3);

	// Get the syscall parameters
	size_t index = 0;
	binput_inst_t ** inputs = malloc0(sizeof(block_inst_t *) * (GROUP_MAX+1));

	luaL_getmetatable(L, "MaxKernel.kernentry");
	lua_pushnil(L);
	while (lua_next(L, 2) != 0)
	{
		if (index == GROUP_MAX)
		{
			return luaL_error(L, "Too many syscall member items (max = %d). Consider increasing value GROUP_MAX in luaenv.c", GROUP_MAX);
		}

		if (!lua_isuserdata(L, -1) || !lua_getmetatable(L, -1) || !lua_rawequal(L, -1, -4))
		{
			return luaL_error(L, "Encountered unknown value for item %s in syscall table (%s)", lua_tostring(L, -3), lua_typename(L, lua_type(L, -2)));
		}
		lua_pop(L, 1);

		kernentry_t * entry = lua_touserdata(L, -1);
		if (strlen(entry->name) == 0)
		{
			return luaL_error(L, "Invalid type for item %s in syscall member table", lua_tostring(L, -2));
		}

		const block_inst_t * in_inst = NULL;
		switch (entry->type)
		{
			case K_BLOCKINST:
			{
				in_inst = entry->data;
				break;
			}

			case K_MODULE:
			{
				const module_t * modobject = entry->data;

				bool found = false;
				list_t * pos;
				list_foreach(pos, &modobject->block_inst)
				{
					block_inst_t * inst = list_entry(pos, block_inst_t, module_list);
					if (strcmp(STATIC_STR, inst->block->name) == 0)
					{
						in_inst = inst;
						found = true;
						break;
					}
				}

				if (!found)
				{
					return luaL_error(L, "Invalid module for item %s in syscall member table. Module does not have a static block.", lua_tostring(L, -2));
				}

				break;
			}
		}

		list_t * pos;
		binput_inst_t * bin = NULL;
		list_foreach(pos, &in_inst->inputs_inst)
		{
			binput_inst_t * test = list_entry(pos, binput_inst_t, block_inst_list);
			if (strcmp(entry->name, test->input->name) == 0)
			{
				bin = test;
				break;
			}
		}

		if (bin == NULL)
		{
			return luaL_error(L, "Invalid syscall input %s.%s for syscall %s", in_inst->block->name, entry->name, name);
		}

		inputs[index++] = bin;

		lua_pop(L, 1);
	}

	//binput_inst_t ** inputs = getinput(L, 2);

	string_t desc = string_blank();
	if (inputs[0] != NULL)
	{
		string_append(&desc, "%s ::", (initial_desc != NULL)? initial_desc : "[no description]");
		size_t i = 0;
		while (inputs[i] != NULL)
		{
			const char * idesc = inputs[i]->input->desc;
			string_append(&desc, " P%zu (%s): %s.", i+1, kernel_datatype(inputs[i]->input->sig), (idesc != NULL)? idesc : "(no desc)");
			i++;
		}
	}
	else
	{
		LUALOG(LOG_WARN, "Cannot create syscall %s with empty group table", name);
		free(inputs);
		return 0;
	}

	syscallblock_t * sb = syscallblock_new(name, inputs, desc.string);
	// TODO - error check: syscallblock_new may return NULL

	free(inputs);

	// Return the syscall block_inst
	kernentry_t * entry = lua_newuserdata(L, sizeof(kernentry_t));
	entry->type = K_BLOCKINST;
	entry->name[0] = '\0';
	entry->data = syscallblock_getblockinst(sb);

	luaL_getmetatable(L, "MaxKernel.kernentry");
	lua_setmetatable(L, -2);

	return 1;
}


static const struct luaL_Reg module_mt[] = {
	{"__index", mt_module_index},
	{NULL, NULL}
};

static const struct luaL_Reg kernentry_mt[] = {
	{"__index", mt_kernentry_index},
	{NULL, NULL}
};

static const struct luaL_Reg config_mt[] = {
	{"__index", mt_config_index},
	{"__newindex", mt_config_newindex},
	{NULL, NULL}
};

bool lua_execfile(const char * path)
{
	bool ret = true;

	LUALOG(LOG_DEBUG, "Creating lua environment");
	lua_State * L = luaL_newstate();
	luaL_openlibs(L);

	// TODO - add lua_atpanic

	// Creating the metatables
	luaL_newmetatable(L, "MaxKernel.module");
	luaL_register(L, NULL, module_mt);
	luaL_newmetatable(L, "MaxKernel.kernentry");
	luaL_register(L, NULL, kernentry_mt);
	luaL_newmetatable(L, "MaxKernel.config");
	luaL_register(L, NULL, config_mt);

	// Add path variable
	const char * bigpath = getpath();
	lua_pushstring(L, bigpath);
	lua_setglobal(L, "path");

	// Register the functions
	lua_register(L, "log", l_log);
	lua_register(L, "print", l_log);
	lua_register(L, "warn", l_warn);
	lua_register(L, "debug", l_debug);
	lua_register(L, "include", l_include);
	lua_register(L, "dofile", l_include);
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
}
