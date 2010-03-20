#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dlfcn.h>

#include "kernel.h"
#include "kernel-priv.h"


#define MODULE_KERNEL_NAME			"__kernel"

extern List * modules;
extern GHashTable * syscalls;
extern List * calentries;
extern module_t * kernel_module;


static char * module_info(void * object)
{
	char * str = "[PLACEHOLDER MODULE INFO]";
	return strdup(str);
}

static void module_destroy(void * object)
{
	module_t * module = object;

	//remove from modules list
	unsigned int index = list_indexof(modules, module);
	if (index >= 0)
	{
		List * nth = list_get(modules, index);
		nth->data = NULL;
	}

	if (module->destroy != NULL)
	{
		LOGK(LOG_DEBUG, "Calling destroy function for module %s", module->path);
		module->destroy();
	}

	FREE(module->path);
	FREE(module->version);
	FREE(module->author);
	FREE(module->description);

	//do not dlclose module because that address space might still be in use
}

static boolean module_symbol(void * module, const char * name, void ** function_ptr)
{
	void * fcn = *function_ptr = dlsym(module, name);
	return fcn != NULL;
}

module_t * module_get(const char * name)
{
	const char * path = resolvepath(name);
	if (path == NULL)
	{
		return NULL;
	}

	module_t * module = NULL;

	List * next = modules;
	while (next != NULL)
	{
		module_t * tester = next->data;
		if (strcmp(tester->path, path) == 0)
		{
			module = tester;
			break;
		}

		next = next->next;
	}

	FREES(path);
	return module;
}

gboolean module_exists(const char * name)
{
	return module_get(name) != NULL;
}

module_t * module_load(const char * name)
{
	const char * path = resolvepath(name);
	if (path == NULL)
	{
		return NULL;
	}

	module_t * module = NULL;

	if ((module = module_get(path)) != NULL)
	{
		return module;
	}

	if (path != NULL)
	{
		LOGK(LOG_DEBUG, "Resolved module %s to path %s", name, path);

		meta_t * meta = NULL;
		List * next;
		GHashTableIter itr;
		
		LOGK(LOG_DEBUG, "Loading module %s", name);
		meta = meta_parse(path);
		
		if (meta == NULL)
		{
			LOGK(LOG_ERR, "Error parsing meta info on module %s", path);
			goto end;
		}
		
		const char * name = strrchr(path, '/');
		if (name == NULL)
			name = path;
		else
			name++; //move one past

		module = kobj_new("Module", strdup(name), module_info, module_destroy, sizeof(module_t));
		module->path = strdup(path);
		module->version = meta->version;
		module->author = meta->author;
		module->description = meta->description;
		module->calentries = g_hash_table_new(g_str_hash, g_str_equal);

		//load mod dependencies
		next = meta->dependencies;
		while (next != NULL)
		{
			const char * modname = next->data;
			LOGK(LOG_DEBUG, "Loading module dependency %s", modname);
			if (!module_load(modname))
			{
				LOGK(LOG_ERR, "Failed to load module dependency %s!", modname);
				return NULL;
			}
			
			free(next->data);
			next = next->next;
		}
		list_free(meta->dependencies);
		
		//now load the mod
		module->module = dlopen(path, RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND);
		if (!module->module)
		{
			LOGK(LOG_FATAL, "Could not load module %s", dlerror());
			//will exit program
		}
		
		//load functions
		if (meta->initialize != NULL && !module_symbol(module->module, meta->initialize, (void **)&module->initialize))
		{
			LOGK(LOG_FATAL, "Could not read module initialize function %s for module %s", meta->initialize, name);
			//will exit program
		}
		if (meta->preactivate != NULL && !module_symbol(module->module, meta->preactivate, (void **)&module->preactivate))
		{
			LOGK(LOG_FATAL, "Could not read module preactivate function %s for module %s", meta->preactivate, name);
			//will exit program
		}
		if (meta->postactivate != NULL && !module_symbol(module->module, meta->postactivate, (void **)&module->postactivate))
		{
			LOGK(LOG_FATAL, "Could not read module postactivate function %s for module %s", meta->postactivate, name);
			//will exit program
		}
		if (meta->destroy != NULL && !module_symbol(module->module, meta->destroy, (void **)&module->destroy))
		{
			LOGK(LOG_FATAL, "Could not read module destroy function %s for module %s", meta->destroy, name);
			//will exit program
		}
		g_free(meta->initialize);
		g_free(meta->preactivate);
		g_free(meta->postactivate);
		g_free(meta->destroy);

		//call preactivate
		if (module->preactivate != NULL)
		{
			module->preactivate();
		}

		//register syscalls
		next = meta->syscalls;
		while (next != NULL)
		{
			syscall_t * syscall = next->data;
			
			LOGK(LOG_DEBUG, "Registering syscall %s with sig %s", syscall->name, syscall->sig);
			
			if (!module_symbol(module->module, syscall->name, (void **)&syscall->func))
			{
				LOGK(LOG_FATAL, "Could not read syscall module symbol for syscall %s", syscall->name);
				//will exit program
			}
			
			syscall->module = module;
			module->syscalls = list_append(module->syscalls, syscall);

			syscall_reg(syscall);
			
			next = next->next;
		}
		list_free(meta->syscalls);
		
		//register config entries
		next = meta->cfgentries;
		while (next != NULL)
		{
			cfgentry_t * cfg = next->data;
			cfg->module = module;

			LOGK(LOG_DEBUG, "Registering configuration entry '%s' with sig %c", cfg->name, cfg->type);

			if (!module_symbol(module->module, cfg->name, (void **)&cfg->variable))
			{
				LOGK(LOG_FATAL, "Could not read configuration module symbol for entry %s", cfg->name);
				//will exit program
			}

			next = next->next;
		}
		module->cfgentries = meta->cfgentries;

		//register calibration entries
		next = meta->calentries;
		while (next != NULL)
		{
			if (meta->calupdate == NULL)
			{
				LOGK(LOG_FATAL, "Module %s defines calibration entries, but no CAL_UPDATE function specified", name);
				//will exit
			}

			calentry_t * cal = next->data;
			cal->module = module;
			
			LOGK(LOG_DEBUG, "Registering calibration entry %s with sig %s", cal->name, cal->sig);
			
			if (!module_symbol(module->module, cal->name, (void **)&cal->active))
			{
				LOGK(LOG_FATAL, "Could not read calibration module symbol for entry %s", cal->name);
				//will exit program
			}
			
			calparam_t * value = cal_getparam(cal->name, cal->sig);
			switch (cal->sig[0])
			{
				#define __module_load_elem(t1, t2, t3) \
					case t1: { \
						cal->deflt = malloc(sizeof(t2)); \
						*(t2 *)cal->deflt = *(t2 *)cal->active; \
						if (value != NULL) { \
							*(t2 *)cal->active = t3; \
						} \
						break; }

				__module_load_elem(T_INTEGER, int, atoi(value->value));
				__module_load_elem(T_DOUBLE, double, strtod(value->value, NULL));

				default:
					LOGK(LOG_FATAL, "Unknown calibration type %c", value->type);
					break;
			}

			if (value != NULL)
			{
				cal_freeparam(value);
			}
			
			g_hash_table_insert(module->calentries, cal->name, cal);
			calentries = list_append(calentries, cal);
			
			next = next->next;
		}
		list_free(meta->calentries);
		
		//register calibration update function
		{
			char * update = meta->calupdate;

			if (update != NULL)
			{
				LOGK(LOG_DEBUG, "Registering calibration update function %s", update);

				if (!module_symbol(module->module, update, (void **)&module->calupdate))
				{
					LOGK(LOG_FATAL, "Could not read calibration update module symbol %s", update);
					//will exit program
				}

				free(update);
			}
		}

		/*
		//register clocks
		next = meta->clocks;
		while (next != NULL)
		{
			mclock_t * clock = next->data;
			clock->update_freq_hz = math_eval(clock->update_freq_str);

			handler_f updatefunc;

			LOGK(LOG_DEBUG, "Registering module clock with function %s and update %f hertz", clock->updatefunc, clock->update_freq_hz);

			if (!module_symbol(module->module, clock->updatefunc, (void **)&updatefunc))
			{
				LOGK(LOG_FATAL, "Could not read clock function module symbol %s", clock->updatefunc);
				//will exit program
			}

			GString * namestr = g_string_new("");
			g_string_printf(namestr, "Clock calling %s->%s", name, clock->updatefunc);

			trigger_t * trigger = trigger_newclock(name, clock->update_freq_hz);
			runnable_t * runnable = exec_newfunction(name, updatefunc, NULL);
			kthread_t * kth = kthread_new(g_string_free(namestr, FALSE), trigger, runnable, 0);
			kthread_schedule(kth);

			next = next->next;
		}
		*/

		//register blocks
		g_hash_table_iter_init(&itr, meta->blocks);
		block_t * block;
		while (g_hash_table_iter_next(&itr, NULL, (gpointer *)&block))
		{
			GString * obj_name = g_string_new("");
			g_string_printf(obj_name, "%s:%s", module->kobject.obj_name, block->name);

			block->module = module;
			block->kobject.obj_name = g_string_free(obj_name, FALSE);
			block->kobject.info = io_blockinfo;
			block->kobject.destructor = io_blockfree;

			LOGK(LOG_DEBUG, "Registering block name %s with onupdate function %s", block->name, block->onupdate_name);
			if (block->onupdate_name == NULL || !module_symbol(module->module, block->onupdate_name, (void **)&block->onupdate))
			{
				LOGK(LOG_FATAL, "Could not read block onupdate function module symbol %s", block->onupdate_name);
				//will exit
			}

			if (block->ondestroy_name != NULL)
			{
				LOGK(LOG_DEBUG, "Registering block name %s with ondestroy function %s", block->name, block->ondestroy_name);
				if (!module_symbol(module->module, block->ondestroy_name, (void **)&block->ondestroy))
				{
					LOGK(LOG_FATAL, "Could not read block ondestroy function module symbol %s", block->ondestroy_name);
					//will exit
				}
			}

			if (block->new_name != NULL)
			{
				LOGK(LOG_DEBUG, "Registering block name %s with constructor %s and sig %s", block->name, block->new_name, block->new_sig);
				if (!module_symbol(module->module, block->new_name, (void **)&block->new))
				{
					LOGK(LOG_FATAL, "Could not read block constructor module symbol %s", block->new_name);
					//will exit
				}
			}

			//reg inputs
			next = block->inputs;
			while (next != NULL)
			{
				bio_t * in = next->data;

				in->block = block;
				LOGK(LOG_DEBUG, "Registering input %s of type '%c' to block %s in module %s", in->name, in->sig, block->name, name);

				next = next->next;
			}

			//reg outputs
			next = block->outputs;
			while (next != NULL)
			{
				bio_t * out = next->data;

				out->block = block;
				LOGK(LOG_DEBUG, "Registering output %s of type '%c', to block %s in module %s", out->name, out->sig, block->name, name);

				next = next->next;
			}

			if (strcmp(block->name, STATIC_STR) == 0)
			{
				//this is a global (static) block, set it up now
				LOGK(LOG_DEBUG, "Initializing global block in module %s", name);

				block_inst_t * blk_inst = io_newblock(block, NULL);
				if (blk_inst == NULL)
				{
					LOG(LOG_FATAL, "Could not create global block instance in module %s", name);
					//will exit
				}

				module->block_global = block;
				module->block_global_inst = blk_inst;
			}

			kobj_register((kobject_t *)block);
		}
		module->blocks = meta->blocks;

		//call postactivate
		if (module->postactivate != NULL)
		{
			module->postactivate();
		}

		modules = list_append(modules, module);

end:
		if (meta != NULL)
			g_free(meta);
		FREES(path);
		
		return module;
	}
	else
	{
		LOGK(LOG_WARN, "Could not load module %s", name);
		return NULL;
	}
}

void module_kernelinit()
{
	module_t * module = kobj_new("Module", strdup(MODULE_KERNEL_NAME), module_info, module_destroy, sizeof(module_t));
	module->path = strdup(MODULE_KERNEL_NAME);
	module->version = strdup("0");
	module->author = strdup("");
	module->description = strdup("");
	module->calentries = g_hash_table_new(g_str_hash, g_str_equal);
	module->blocks = g_hash_table_new(g_str_hash, g_str_equal);

	kernel_module = module;
	modules = list_append(modules, module);
}

void module_init(const module_t * module)
{
	if (module->initialize == NULL)
		return;

	//now call it
	LOGK(LOG_DEBUG, "Calling initializer function for module %s", module->kobject.obj_name);
	module->initialize();
}

int module_compare(const void * a, const void * b)
{
	const module_t * ma = a;
	const module_t * mb = b;
	return strcmp(ma->kobject.obj_name, mb->kobject.obj_name);
}

