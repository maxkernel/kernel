#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <glib.h>

#include "kernel.h"
#include "kernel-priv.h"
#include "serialize.h"

extern char ** path;

#define checkparams(L, func, types, param_len)		checkparams2(L, func, types, param_len, param_len)
static void checkparams2(lua_State * L, const char * func, int * types, size_t minparams, size_t maxparams)
{
	if (lua_gettop(L) < minparams || lua_gettop(L) > maxparams)
	{
		if (minparams == maxparams)
		{
			luaL_error(L, "Invalid parameter length for function '%s'. Expected %d, got %d", func, minparams, lua_gettop(L));
		}
		else
		{
			luaL_error(L, "Invalid parameter length for function '%s'. Expected %d to %d, got %d", func, minparams, maxparams, lua_gettop(L));
		}
	}

	size_t i = 1;
	for (; i<=lua_gettop(L); i++)
	{
		if (lua_type(L, i) != types[i-1])
		{
			luaL_error(L, "Parameter %d invalid for function '%s'. Expected %s, got %s", i, func, lua_typename(L, types[i-1]), lua_typename(L, lua_type(L, i)));
		}
	}
}

static void pushio(lua_State * L, const char * name, block_inst_t * blk_inst)
{
	lua_pushstring(L, name);
	lua_newtable(L);

	lua_pushstring(L, "name");
	lua_pushstring(L, name);
	lua_settable(L, -3);

	lua_pushstring(L, "__ioblkinst");
	lua_pushlightuserdata(L, blk_inst);
	lua_settable(L, -3);

	lua_settable(L, -3);
}

static void setio(lua_State * L, block_inst_t * blk_inst)
{
	block_t * blk = blk_inst->block;
	List * next = NULL;

	next = blk->inputs;
	while (next != NULL)
	{
		bio_t * in = next->data;
		LOGK(LOG_DEBUG, "Lua: adding '%s' block input %s", blk->name, in->name);

		pushio(L, in->name, blk_inst);
		next = next->next;
	}

	next = blk->outputs;
	while (next != NULL)
	{
		bio_t * out = next->data;
		LOGK(LOG_DEBUG, "Lua: adding '%s' block output %s", blk->name, out->name);

		pushio(L, out->name, blk_inst);
		next = next->next;
	}

	lua_pushstring(L, "__blkinst");
	lua_pushlightuserdata(L, blk_inst);
	lua_settable(L, -3);
}

static void * __getblkinst(lua_State * L, int index)
{
	block_inst_t * inst = NULL;

	lua_getfield(L, index, "__blkinst");
	if (!lua_isnil(L, -1))
	{
		inst = lua_touserdata(L, -1);
	}

	lua_pop(L, 1);
	return inst;
}

static void * __getinput(lua_State * L, int index)
{
	binput_inst_t * input = NULL;

	lua_getfield(L, index, "name");
	lua_getfield(L, index, "__ioblkinst");

	if (!lua_isnil(L, -2) && !lua_isnil(L, -1))
	{
		const char * name = lua_tostring(L, -2);
		block_inst_t * inst = lua_touserdata(L, -1);

		input = g_hash_table_lookup(inst->inputs_inst, name);
	}

	lua_pop(L, 2);
	return input;
}

static List * __getgrouplist(lua_State * L, List * list, void * (*getter)(lua_State * L, int index), int index)
{
	void * v = getter(L, index);
	if (v != NULL)
	{
		list = list_append(list, v);
	}
	else
	{
		int onindex = 0;

		lua_pushnil(L);
		while (lua_next(L, index))
		{
			if (!lua_istable(L, -1))
			{
				luaL_error(L, "Encountered unknown value in group table (%s)", lua_typename(L, lua_type(L, -1)));
			}

			list = __getgrouplist(L, list, getter, lua_gettop(L));
			onindex = lua_tointeger(L, -2);
			lua_pop(L, 1);
		}
	}

	return list;
}

static block_inst_t ** getinsts(lua_State * L, int tableindex)
{
	List * list = __getgrouplist(L, NULL, __getblkinst, tableindex);

	block_inst_t ** insts = g_malloc0(sizeof(block_inst_t *) * (list_length(list) + 1));

	List * next = list;
	int index = 0;

	while (next != NULL)
	{
		insts[index++] = next->data;
		next = next->next;
	}

	list_free(list);
	return insts;
}

static binput_inst_t ** getinput(lua_State * L, int tableindex)
{
	List * list = __getgrouplist(L, NULL, __getinput, tableindex);

	binput_inst_t ** inputs = g_malloc0(sizeof(binput_inst_t *) * (list_length(list) + 1));

	List * next = list;
	int index = 0;

	while (next != NULL)
	{
		inputs[index++] = next->data;
		next = next->next;
	}

	list_free(list);
	return inputs;
}

static int l_log(lua_State * L)
{
	LOG(LOG_INFO, "Lua: %s", lua_tostring(L, 1));
	return 0;
}

static int l_warn(lua_State * L)
{
	LOG(LOG_WARN, "Lua: %s", lua_tostring(L, 1));
	return 0;
}

static int l_debug(lua_State * L)
{
	LOG(LOG_DEBUG, "Lua: %s", lua_tostring(L, 1));
	return 0;
}

static int l_include(lua_State * L)
{
	int p[] = { LUA_TSTRING };
	checkparams(L, "include", p, 1);

	const char * file = lua_tostring(L, 1);

	lua_getglobal(L, "path");
	const char * newpath = lua_tostring(L, -1);
	setpath(newpath);

	const char * filepath = resolvepath(file);
	if (filepath == NULL)
	{
		luaL_error(L, "Include: Could not resolve file %s", file);
	}
	else
	{
		if (luaL_loadfile(L, filepath) || lua_pcall(L, 0,0,0))
		{
			luaL_error(L, "%s", lua_tostring(L, -1));
			lua_pop(L, 1);
		}
		FREES(filepath);
	}

	return 0;
}

static int l_newblock(lua_State * L)
{
	block_t * blk = lua_touserdata(L, lua_upvalueindex(1));
	const char * sig = blk->new_sig;

	void ** params = g_malloc(sizeof(void *) * strlen(sig));

	//build constructor args
	int stack = 1;
	int index = 0;
	for (; index < strlen(sig); index++)
	{
		switch (sig[index])
		{
			#define __l_newblock_elem(t1, t2, t, f) \
				case t1: { \
					if (lua_isnone(L, stack)) { \
						luaL_error(L, "Parameter %d of block %s in module %s must be of type %s", stack, blk->name, blk->module->path, t);\
					} \
					t2 * v = g_malloc(sizeof(t2)); \
					*v = (t2) f(L, stack); \
					params[index] = v; \
					break; }

			__l_newblock_elem(T_BOOLEAN, boolean, "boolean", lua_toboolean)
			__l_newblock_elem(T_INTEGER, int, "integer", lua_tointeger)
			__l_newblock_elem(T_DOUBLE, double, "double", lua_tonumber)
			__l_newblock_elem(T_STRING, char *, "string", lua_tostring)

			case T_CHAR:
			{
				if (lua_isnone(L, stack))
				{
					luaL_error(L, "Parameter %d of block %s must be of type string", stack, blk->name);
				}

				char * v = g_malloc(sizeof(char));
				*v = lua_tostring(L, stack)[0];
				params[index] = v;
				break;
			}
		}

		stack++;
	}

	block_inst_t * blk_inst = io_newblock(blk, params);
	if (blk_inst == NULL)
	{
		luaL_error(L, "Could not create block instance of block %s in module %s", blk->name, blk->module->path);
	}

	for (index=0; index<strlen(sig); index++)
	{
		g_free(params[index]);
	}
	g_free(params);

	lua_newtable(L);
	setio(L, blk_inst);

	return 1;
}

static int l_config__index(lua_State * L)
{
	lua_pushstring(L, "__modulepath");
	lua_gettable(L, 1);

	const char * modname = lua_tostring(L, -1);
	const char * key = lua_tostring(L, 2);

	cfgentry_t * cfg = cfg_getparam(modname, key);
	if (cfg == NULL)
	{
		lua_pushnil(L);
	}
	else
	{
		switch (cfg->type)
		{
			#define __l_config__index_elem(t1, t2, f) \
				case t1: \
					f(L, *(t2 *)cfg->variable);\
					break;

			__l_config__index_elem(T_BOOLEAN, boolean, lua_pushboolean)
			__l_config__index_elem(T_INTEGER, int, lua_pushinteger)
			__l_config__index_elem(T_DOUBLE, double, lua_pushnumber)
			__l_config__index_elem(T_STRING, char *, lua_pushstring)

			case T_CHAR:
			{
				char str[2] = { *(char *)cfg->variable, '\0' };
				lua_pushstring(L, str);
				break;
			}
		}
	}

	return 1;
}

static int l_config__newindex(lua_State * L)
{
	lua_pushstring(L, "__modulepath");
	lua_gettable(L, 1);

	const char * modname = lua_tostring(L, -1);
	const char * key = lua_tostring(L, 2);
	const char * value = lua_tostring(L, 3);

	cfg_setparam(modname, key, value);

	return 0;
}

static int l_loadmodule(lua_State * L)
{
	int p[] = { LUA_TSTRING };
	checkparams(L, "loadmodule", p, 1);

	const char * modname = lua_tostring(L, 1);

	lua_getglobal(L, "path");
	const char * newpath = lua_tostring(L, -1);
	setpath(newpath);

	module_t * mod = module_load(modname);
	if (mod == NULL)
	{
		LOGK(LOG_FATAL, "Lua: Load Module: Could not load module %s", modname);
		return 0;
	}

	lua_newtable(L);	//return value

	//set module entry
	lua_pushstring(L, "__module");
	lua_pushlightuserdata(L, mod);
	lua_settable(L, -3);

	//add global block io
	if (mod->block_global_inst != NULL)
	{
		setio(L, mod->block_global_inst);
	}

	//add instance block constructors
	{
		GHashTableIter itr;
		block_t * blk;

		g_hash_table_iter_init(&itr, mod->blocks);
		while (g_hash_table_iter_next(&itr, NULL, (gpointer *)&blk))
		{
			LOGK(LOG_DEBUG, "Lua: adding block constructor for %s", blk->name);
			lua_pushstring(L, blk->name);
			lua_newtable(L);

			lua_pushstring(L, "new");
			lua_pushlightuserdata(L, blk);
			lua_pushcclosure(L, l_newblock, 1);
			lua_settable(L, -3);

			lua_settable(L, -3);
		}
	}

	//set config table (as proxy w/ meta table)
	lua_pushstring(L, "config");
	lua_newtable(L);

	//set config["__modulepath"]
	lua_pushstring(L, "__modulepath");
	lua_pushstring(L, modname);
	lua_settable(L, -3);

	lua_newtable(L); //meta table
	lua_pushstring(L, "__index");
	lua_pushcfunction(L, l_config__index);
	lua_settable(L, -3);
	lua_pushstring(L, "__newindex");
	lua_pushcfunction(L, l_config__newindex);
	lua_settable(L, -3);
	lua_setmetatable(L, -2);

	//attach config table
	lua_settable(L, -3);


	return 1;
}

static int l_route(lua_State * L)
{
	int p[] = { LUA_TTABLE, LUA_TTABLE };
	checkparams(L, "route", p, 2);

	lua_getfield(L, 1, "__ioblkinst");
	lua_getfield(L, 2, "__ioblkinst");
	block_inst_t * out_inst = lua_touserdata(L, -2);
	block_inst_t * in_inst = lua_touserdata(L, -1);

	lua_getfield(L, 1, "name");
	lua_getfield(L, 2, "name");
	const char * out = lua_tostring(L, -2);
	const char * in = lua_tostring(L, -1);

	if (out_inst == NULL || in_inst == NULL)
	{
		luaL_error(L, "Could not route IO, invalid Block instance");
	}

	boutput_inst_t * bout = g_hash_table_lookup(out_inst->outputs_inst, out);
	binput_inst_t * bin = g_hash_table_lookup(in_inst->inputs_inst, in);

	if (bout == NULL)
	{
		luaL_error(L, "Could not route %s.%s, Not an output!", out_inst->block->name, out);
	}

	if (bin == NULL)
	{
		luaL_error(L, "Could not route %s.%s, Not an input!", in_inst->block->name, in);
	}


	if (!io_route(bout, bin))
	{
		luaL_error(L, "Failed to route IO %s.%s to %s.%s", out_inst->block->name, out, in_inst->block->name, in);
	}

	return 0;
}

static int l_newrategroup(lua_State * L)
{
	int p[] = { LUA_TSTRING, LUA_TTABLE, LUA_TNUMBER };
	checkparams(L, "rategroup", p, 3);

	const char * name = lua_tostring(L, 1);
	double freq_hz = lua_tonumber(L, 3);

	//get all the block instances
	block_inst_t ** insts = getinsts(L, 2);

	//print some debug info
	if (insts[0] != NULL)
	{
		String str = string_new("Creating new rategroup %s with update @ %f Hz and members: %s", name, freq_hz, insts[0]->block->name);

		size_t i = 1;
		while (insts[i] != NULL)
		{
			string_append(&str, ", %s", insts[i]->block->name);
			i++;
		}

		LOGK(LOG_DEBUG, "%s", str.string);
	}
	else
	{
		LOGK(LOG_WARN, "Cannot create rategroup %s with no members", name);
		return 0;
	}

	//create new rate group
	runnable_t * rungrp = exec_newrungroup(name, (runnable_t **)insts);
	trigger_t * trigger = trigger_newclock(name, freq_hz);

	String name_str = string_new("%s rategroup", name);
	kthread_t * kth = kthread_new(string_copy(&name_str), trigger, rungrp, 0);
	kthread_schedule(kth);

	return 0;
}

static int l_newvrategroup(lua_State * L)
{
	if (lua_isnoneornil(L, 3))
	{
		int p[] = { LUA_TSTRING, LUA_TTABLE, LUA_TNIL, LUA_TNUMBER };
		checkparams2(L, "varrategroup", p, 2, 4);
	}
	else
	{
		int p[] = { LUA_TSTRING, LUA_TTABLE, LUA_TTABLE, LUA_TNUMBER };
		checkparams2(L, "varrategroup", p, 3, 4);
	}

	const char * name = lua_tostring(L, 1);
	double freq_hz = lua_tonumber(L, 4);
	boutput_inst_t * out = NULL;

	if (!lua_isnoneornil(L, 3))
	{
		lua_getfield(L, 3, "__ioblkinst");
		lua_getfield(L, 3, "name");
		if (!lua_isnil(L, -2) && !lua_isnil(L, -1))
		{
			block_inst_t * out_inst = lua_touserdata(L, -2);
			const char * outname = lua_tostring(L, -1);

			if (out_inst == NULL)
			{
				luaL_error(L, "Invalid Block instance");
			}

			out = g_hash_table_lookup(out_inst->outputs_inst, outname);

			if (out == NULL)
			{
				luaL_error(L, "Error: %s.%s is not an output!", out_inst->block->name, outname);
			}
		}
		else
		{
			luaL_error(L, "Invalid rategroup rate-controlling output (param 3)");
		}
		lua_pop(L, 2);
	}

	//get all the block instances
	block_inst_t ** insts = getinsts(L, 2);

	//print some debug info
	if (insts[0] != NULL)
	{
		String str = string_new("Creating new vrategroup with initial update @ %f Hz and members: %s", freq_hz, insts[0]->block->name);
		size_t i = 1;
		while (insts[i] != NULL)
		{
			string_append(&str, ", %s", insts[i]->block->name);
			i++;
		}
		LOGK(LOG_DEBUG, "%s", str.string);
	}
	else
	{
		LOGK(LOG_WARN, "Cannot create vrategroup %s with no members", name);
		return 0;
	}

	//create new rate group
	runnable_t * rungrp = exec_newrungroup(name, (runnable_t **)insts);
	trigger_t * trigger = trigger_newvarclock(name, freq_hz);

	String name_str = string_new("%s vrategroup", name);
	kthread_t * kth = kthread_new(string_copy(&name_str), trigger, rungrp, 0);

	kthread_schedule(kth);

	if (out != NULL)
	{
		io_route(out, trigger_varclock_getrateinput(trigger));
	}

	lua_newtable(L);
	setio(L, trigger_varclock_getblockinst(trigger));

	return 1;
}

static int l_newsyscall(lua_State * L)
{
	int p[] = { LUA_TSTRING, LUA_TTABLE, LUA_TSTRING };
	checkparams2(L, "newsyscall", p, 2, 3);

	const char * name = lua_tostring(L, 1);
	const char * initial_desc = lua_tostring(L, 3);

	//get the syscall parameters
	binput_inst_t ** inputs = getinput(L, 2);
	if (inputs[0] == NULL)
	{
		luaL_error(L, "Empty group table");
	}

	GString * desc = g_string_new(initial_desc != NULL? initial_desc : "[no description]");
	g_string_append_printf(desc, " ::");

	size_t i=0;
	while (inputs[i] != NULL)
	{
		const char * idesc = inputs[i]->input->desc;
		g_string_append_printf(desc, " P%d (%s): %s.", i+1, kernel_datatype(inputs[i]->input->sig), idesc != NULL? idesc : "(no desc)");
		i++;
	}

	syscallblock_t * sb = syscallblock_new(name, inputs, desc->str);

	g_string_free(desc, TRUE);
	g_free(inputs);

	lua_newtable(L);
	setio(L, syscallblock_getblockinst(sb));

	return 1;
}

boolean lua_execfile(const char * name)
{
	boolean ret = TRUE;
	const char * filepath = resolvepath(name);
	if (filepath == NULL)
	{
		LOGK(LOG_FATAL, "Could not execute lua file %s", name);
	}

	LOGK(LOG_DEBUG, "Creating lua environment");
	lua_State * L = luaL_newstate();
	luaL_openlibs(L);

	//add path var
	const char * bigpath = getpath();
	lua_pushstring(L, bigpath);
	lua_setglobal(L, "path");

	lua_register(L, "log", l_log);
	lua_register(L, "print", l_log);
	lua_register(L, "warn", l_warn);
	lua_register(L, "debug", l_debug);
	lua_register(L, "include", l_include);
	lua_register(L, "dofile", l_include);
	lua_register(L, "loadmodule", l_loadmodule);
	lua_register(L, "route", l_route);
	lua_register(L, "newrategroup", l_newrategroup);
	lua_register(L, "newvrategroup", l_newvrategroup);
	lua_register(L, "newsyscall", l_newsyscall);

	LOGK(LOG_DEBUG, "Executing lua file %s", filepath);
	if (luaL_loadfile(L, filepath) || lua_pcall(L, 0,0,0))
	{
		LOGK(LOG_ERR, "Lua: %s", lua_tostring(L, -1));
		lua_pop(L, 1);
		ret = FALSE;
	}

	FREES(filepath);
	lua_close(L);
	return ret;
}
