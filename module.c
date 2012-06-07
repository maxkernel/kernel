#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dlfcn.h>

//#include <glib.h>

#include <kernel.h>
#include <kernel-priv.h>


extern list_t modules;
//extern list_t calentries;
//extern GHashTable * syscalls;
//extern module_t * kernel_module;


static char * module_info(void * object)
{
	char * str = "[PLACEHOLDER MODULE INFO]";
	return strdup(str);
}

static void module_destroy(void * object)
{
	module_t * module = object;

	// Sanity check
	{
		if (module == NULL)
		{
			LOGK(LOG_ERR, "Module is NULL (module_destroy)");
			return;
		}
	}

	//remove from modules list
	list_remove(&module->global_list);

	// Call destroyer
	{
		const meta_t * meta = module->backing;
		meta_destroyer destroyer = NULL;
		meta_getactivators(meta, NULL, &destroyer, NULL, NULL);

		if (destroyer != NULL)
		{
			LOGK(LOG_DEBUG, "Calling destroy function for module %s", module->kobject.object_name);
			destroyer();
		}
	}

	//do not dlclose module because that address space might still be in use
	// TODO - we can check that and unload if not in use!
}

/*
static bool module_symbol(void * module, const char * name, void ** function_ptr)
{
	void * fcn = *function_ptr = dlsym(module, name);
	return fcn != NULL;
}
*/

/*
module_t * module_get(const char * name)
{
	const char * prefix = path_resolve(name, P_MODULE);
	if (prefix == NULL)
	{
		return NULL;
	}

	string_t path = path_join(prefix, name);
	if (!strsuffix(path.string, ".mo"))
	{
		string_append(&path, ".mo");
	}

	list_t * pos;
	module_t * module = NULL;
	list_foreach(pos, &modules)
	{
		module_t * tester = list_entry(pos, module_t, global_list);
		if (strcmp(tester->path, path.string) == 0)
		{
			module = tester;
			break;
		}
	}

	return module;
}
*/

module_t * module_lookup(const char * name)
{
	const char * prefix = path_resolve(name, P_MODULE);
	if (prefix == NULL)
	{
		return NULL;
	}

	string_t path = path_join(prefix, name);
	if (!strsuffix(path.string, ".mo"))
	{
		string_append(&path, ".mo");
	}

	list_t * pos;
	list_foreach(pos, &modules)
	{
		module_t * tester = list_entry(pos, module_t, global_list);

		const char * testerpath = NULL;
		meta_getinfo(tester->backing, &testerpath, NULL, NULL, NULL, NULL);

		if (testerpath != NULL && strcmp(testerpath, path.string) == 0)
		{
			return tester;
		}
	}

	return NULL;
}

block_t * module_lookupblock(module_t * module, const char * blockname)
{
	// Sanity check
	{
		if unlikely(module == NULL || blockname == NULL)
		{
			return NULL;
		}
	}

	list_t * pos;
	list_foreach(pos, &module->blocks)
	{
		block_t * block = list_entry(pos, block_t, module_list);
		if (strcmp(blockname, block->name) == 0)
		{
			return block;
		}
	}

	return NULL;
}

/*
const block_inst_t * module_getstaticblockinst(const module_t * module)
{
	list_t * pos;
	list_foreach(pos, &module->blockinst)
	{
		block_inst_t * inst = list_entry(pos, block_inst_t, module_list);
		if (strcmp(STATIC_STR, inst->block->name) == 0)
		{
			return inst;
		}
	}

	return NULL;
}
*/

bool module_exists(const char * name)
{
	return module_lookup(name) != NULL;
}

module_t * module_load(meta_t * meta, metalookup_f lookup, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(meta == NULL || lookup == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}

	const char * path = NULL;
	const char * name = NULL;
	const version_t * version = NULL;
	meta_getinfo(meta, &path, &name, &version, NULL, NULL);

	// Check to see if module has already been loaded
	{
		const module_t * existing = module_lookup(path);
		if (existing != NULL)
		{
			// Remove const qualifier
			return (module_t *)existing;
		}
	}

	// Print diagnostic message
	{
		string_t version_str = version_tostring(*version);
		LOGK(LOG_DEBUG, "Loading module %s v%s", path, version_str.string);
	}

	// Handle dependencies
	// TODO IMPORTANT - handle circular dependencies, somehow...
	{
		iterator_t ditr = meta_dependencyitr(meta);
		{
			const meta_dependency_t * dependency = NULL;
			while (meta_dependencynext(ditr, &dependency))
			{
				const char * depname = NULL;
				meta_getdependency(dependency, &depname);

				meta_t * depmeta = lookup(depname);
				if (depmeta == NULL)
				{
					exception_set(err, ENOENT, "Module dependency resolve failed for %s", depname);
					return NULL;
				}

				module_t * depmodule = module_load(depmeta, lookup, err);
				if (depmodule == NULL || exception_check(err))
				{
					return NULL;
				}

				/*
				LOGK(LOG_DEBUG, "Resolving dependency %s", depname);

				const char * prefix = path_resolve(depname, P_MODULE);
				if (prefix == NULL)
				{
					exception_set(err, NOENT, "Could not resolve dependency %s for module %s", depname, path);
					return NULL;
				}

				string_t path = path_join(prefix, depname);
				if (!strsuffix(path.string, ".mo"))
				{
					string_append(&path, ".mo");
				}

				const meta_t * depmeta = NULL;
				if (!model_lookupmeta(model, path.string, &depmeta))
				{
					// Meta not in model, load it in manually
					exception_t * e = NULL;
					meta_t * parsemeta = meta_parseelf(path.string, &e);
					if (parsemeta == NULL || exception_check(&e))
					{
						LOGK(LOG_FATAL, "Load meta failed: %s", (e == NULL)? "Unknown error!" : e->message);
						// Will exit
					}

					model_addmeta((model_t *)model, parsemeta, &e);
					if (exception_check(&e))
					{
						LOGK(LOG_FATAL, "Load meta failed: %s", (e == NULL)? "Unknown error!" : e->message);
						// Will exit
					}

					meta_destroy(parsemeta);

					if (!model_lookupmeta(model, path.string, &depmeta))
					{
						LOGK(LOG_FATAL, "Could not find meta just added to model!");
						// Will exit
					}
				}
				*/

				//load_module(depmeta);
			}
		}
		iterator_free(ditr);
	}

	// Try loading it
	if (!meta_loadmodule(meta, err))
	{
		return NULL;
	}

	module_t * module = kobj_new("Module", name, module_info, module_destroy, sizeof(module_t));
	module->backing = meta;
	LIST_INIT(&module->syscalls);
	LIST_INIT(&module->configs);
	LIST_INIT(&module->blocks);
	LIST_INIT(&module->blockinsts);

	// Create configs
	{
		iterator_t citr = meta_configitr(meta);
		{
			const meta_variable_t * variable = NULL;
			while (meta_confignext(citr, &variable))
			{
				config_t * config = config_new(meta, variable, err);
				if (config == NULL || exception_check(err))
				{
					return NULL;
				}

				list_add(&module->configs, &config->module_list);
			}
		}
		iterator_free(citr);
	}

	// Create syscalls
	{
		iterator_t sitr = meta_syscallitr(meta);
		{
			const meta_callback_t * callback = NULL;
			while(meta_syscallnext(sitr, &callback))
			{
				const char * name = NULL, * sig = NULL, * desc = NULL;
				syscall_f func = NULL;
				meta_getcallback(callback, &name, &sig, &desc, &func);

				syscall_t * syscall = syscall_new(name, sig, func, desc, err);
				if (syscall == NULL || exception_check(err))
				{
					return NULL;
				}

				list_add(&module->syscalls, &syscall->module_list);
			}
		}
		iterator_free(sitr);
	}

	// Create blocks
	{
		const meta_block_t * block = NULL;
		iterator_t bitr = meta_blockitr(meta);
		{
			while (meta_blocknext(bitr, &block))
			{
				const char * block_name = NULL, *new_sig;
				meta_callback_f new = NULL;
				meta_getblock(block, &block_name, NULL, NULL, &new_sig, NULL, &new);

				const meta_blockcallback_t * onupdate = NULL, * ondestroy = NULL;
				meta_lookupblockcbs(meta, block, &onupdate, &ondestroy);

				meta_callback_vp_f onupdate_cb = NULL, ondestroy_cb = NULL;
				meta_getblockcb(onupdate, NULL, NULL, &onupdate_cb);
				meta_getblockcb(ondestroy, NULL, NULL, &ondestroy_cb);

				block_t * blk = block_new(module, block_name, new_sig, new, onupdate_cb, ondestroy_cb, err);
				if (blk == NULL || exception_check(err))
				{
					return NULL;
				}

				list_add(&module->blocks, &blk->module_list);
			}
		}
		iterator_free(bitr);
	}


	list_add(&modules, &module->global_list);
	return module;

	/*
	const char * prefix = path_resolve(name, P_MODULE);
	if (prefix == NULL)
	{
		LOGK(LOG_WARN, "Could not resolve module path for %s", name);
		return NULL;
	}

	string_t path = string_new("%s/%s%s", prefix, name, (strsuffix(name, ".mo"))? "" : ".mo");

	module_t * module = module_get(path.string);
	if (module != NULL)
	{
		return module;
	}

	LOGK(LOG_DEBUG, "Resolved module %s to path %s", name, path.string);

	meta_t * meta = NULL;
	list_t * pos, * q;

	LOGK(LOG_DEBUG, "Loading module %s", name);
	meta = meta_parse(path.string);

	if (meta == NULL)
	{
		LOGK(LOG_ERR, "Error parsing meta info on module %s", path.string);
		goto end;
	}

	const char * nice_name = strrchr(path.string, '/');
	if (nice_name == NULL)
	{
		nice_name = path.string;
	}
	else
	{
		nice_name += 1; //move one past
	}

	module = kobj_new("Module", strdup(nice_name), module_info, module_destroy, sizeof(module_t));
	module->path = string_copy(&path);
	module->version = meta->version;
	module->author = meta->author;
	module->description = meta->description;
	LIST_INIT(&module->syscalls);
	LIST_INIT(&module->cfgentries);
	LIST_INIT(&module->dependencies);
	LIST_INIT(&module->calentries);
	LIST_INIT(&module->blocks);
	LIST_INIT(&module->block_inst);

	// load mod dependencies
	list_foreach_safe(pos, q, &meta->dependencies)
	{
		dependency_t * dep = list_entry(pos, dependency_t, module_list);

		LOGK(LOG_DEBUG, "Loading module dependency %s", dep->modname);
		if ((dep->module = module_load(dep->modname)) == NULL)
		{
			LOGK(LOG_ERR, "Failed to load module dependency %s!", dep->modname);
			return NULL;
		}

		list_remove(&dep->module_list);
		list_add(&module->dependencies, &dep->module_list);
	}

	// now load the mod
	module->module = dlopen(path.string, RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND);
	if (!module->module)
	{
		LOGK(LOG_FATAL, "Could not load module %s", dlerror());
		//will exit program
	}

	// load functions
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
	free(meta->initialize);
	free(meta->preactivate);
	free(meta->postactivate);
	free(meta->destroy);

	// call preactivate
	if (module->preactivate != NULL)
	{
		LOG(LOG_DEBUG, "Calling registered pre-activation function on module %s", name);
		module->preactivate();
	}

	// register syscalls
	list_foreach_safe(pos, q, &meta->syscalls)
	{
		syscall_t * syscall = list_entry(pos, syscall_t, module_list);
		syscall->module = module;

		LOGK(LOG_DEBUG, "Registering syscall %s with sig %s", syscall->name, syscall->sig);
		if (!module_symbol(module->module, syscall->name, (void **)&syscall->func))
		{
			LOGK(LOG_FATAL, "Could not read syscall module symbol for syscall %s", syscall->name);
			//will exit program
		}

		list_remove(&syscall->module_list);
		list_add(&module->syscalls, &syscall->module_list);

		syscall_reg(syscall);
	}

	// register config entries
	list_foreach_safe(pos, q, &meta->cfgentries)
	{
		cfgentry_t * cfg = list_entry(pos, cfgentry_t, module_list);
		cfg->module = module;

		LOGK(LOG_DEBUG, "Registering configuration entry '%s' with sig %c", cfg->name, cfg->type);
		if (!module_symbol(module->module, cfg->name, (void **)&cfg->variable))
		{
			LOGK(LOG_FATAL, "Could not read configuration module symbol for entry %s", cfg->name);
			//will exit program
		}

		list_remove(&cfg->module_list);
		list_add(&module->cfgentries, &cfg->module_list);
	}

	// register calibration entries
	list_foreach_safe(pos, q, &meta->calentries)
	{
		if (meta->calupdate == NULL)
		{
			LOGK(LOG_FATAL, "Module %s defines calibration entries, but no CAL_UPDATE function specified", name);
			//will exit
		}

		calentry_t * cal = list_entry(pos, calentry_t, module_list);
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

			// TODO - fix this switch statement! Also, catch errors on the parse_xxx()
			__module_load_elem(T_INTEGER, int, parse_int(value->value, NULL));
			__module_load_elem(T_DOUBLE, double, parse_double(value->value, NULL));

			default:
				LOGK(LOG_FATAL, "Unknown calibration type %c", value->type);
				break;
		}

		if (value != NULL)
		{
			cal_freeparam(value);
		}

		list_remove(&cal->module_list);
		list_add(&module->calentries, &cal->module_list);
		list_add(&calentries, &cal->global_list);
	}

	// register calibration update function
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

	// register calibration preview function
	{
		char * preview = meta->calpreview;

		if (preview != NULL)
		{
			LOGK(LOG_DEBUG, "Registering calibration preview function %s", preview);

			if (!module_symbol(module->module, preview, (void **)&module->calpreview))
			{
				LOGK(LOG_FATAL, "Could not read calibration preview module symbol %s", preview);
				//will exit program
			}

			free(preview);
		}
	}

	// register blocks
	list_foreach_safe(pos, q, &meta->blocks)
	{
		list_t * pos2;
		block_t * block = list_entry(pos, block_t, module_list);
		string_t obj_name = string_new("%s:%s", module->kobject.object_name, block->name);

		block->module = module;
		block->kobject.object_name = strdup(obj_name.string);
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

		// reg inputs
		list_foreach(pos2, &block->inputs)
		{
			bio_t * in = list_entry(pos2, bio_t, block_list);

			in->block = block;
			LOGK(LOG_DEBUG, "Registering input %s of type '%c' to block %s in module %s", in->name, in->sig, block->name, name);
		}

		//reg outputs
		list_foreach(pos2, &block->outputs)
		{
			bio_t * out = list_entry(pos2, bio_t, block_list);

			out->block = block;
			LOGK(LOG_DEBUG, "Registering output %s of type '%c', to block %s in module %s", out->name, out->sig, block->name, name);
		}

		if (strcmp(block->name, STATIC_STR) == 0)
		{
			// this is a global (static) block, set it up now
			LOGK(LOG_DEBUG, "Initializing global block in module %s", name);

			// TODO - is this the best place to initialize the static block??
			block_inst_t * blk_inst = io_newblockinst(block, NULL);
			if (blk_inst == NULL)
			{
				LOG(LOG_FATAL, "Could not create global block instance in module %s", name);
				//will exit
			}

			// TODO - do we need to save the static block seperately?
			//module->block_static = block;
			//module->block_static_inst = blk_inst;
		}

		kobj_register((kobject_t *)block);
		list_remove(&block->module_list);
		list_add(&module->blocks, &block->module_list);
	}

	//call postactivate
	if (module->postactivate != NULL)
	{
		LOG(LOG_DEBUG, "Calling registered post-activation function on module %s", name);
		module->postactivate();
	}

	list_add(&modules, &module->global_list);

end:
	if (meta != NULL)
		g_free(meta);

	return module;
	*/
}

/*
void module_kernelinit()
{
	module_t * module = kobj_new("Module", strdup(MOD_KERNEL), module_info, module_destroy, sizeof(module_t));
	module->path = strdup(MOD_KERNEL);
	module->version = strdup("0");
	module->author = strdup("");
	module->description = strdup("");
	LIST_INIT(&module->syscalls);
	LIST_INIT(&module->cfgentries);
	LIST_INIT(&module->dependencies);
	LIST_INIT(&module->calentries);
	LIST_INIT(&module->blocks);
	LIST_INIT(&module->block_inst);

	kernel_module = module;
	list_add(&modules, &module->global_list);
}
*/

void module_init(const module_t * module)
{
	// Sanity check
	{
		if (module == NULL)
		{
			LOGK(LOG_ERR, "Module is NULL (module_init)");
			return;
		}
	}

	// Call initializer function
	{
		const meta_t * meta = module->backing;
		meta_initializer initializer = NULL;
		meta_getactivators(meta, &initializer, NULL, NULL, NULL);

		if (initializer != NULL)
		{
			LOGK(LOG_DEBUG, "Calling initializer function for module %s", module->kobject.object_name);

			if (!initializer())
			{
				LOGK(LOG_FATAL, "Module initialization failed (%s).", module->kobject.object_name);
				// Will exit
			}
		}
	}
}

// TODO - rename to module_activate?
void module_act(const module_t * module, moduleact_t act)
{
	// Sanity check
	{
		if (module == NULL)
		{
			LOGK(LOG_ERR, "Module is NULL (module_act)");
			return;
		}
	}

	// Call initializer function
	{
		const meta_t * meta = module->backing;

		meta_activator activator = NULL;
		const char * activator_name = "";

		switch (act)
		{
			case act_preact:
				meta_getactivators(meta, NULL, NULL, &activator, NULL);
				activator_name = "preactivator";
				break;

			case act_postact:
				meta_getactivators(meta, NULL, NULL, NULL, &activator);
				activator_name = "postactivator";
				break;
		}

		if (activator != NULL)
		{
			LOGK(LOG_DEBUG, "Calling %s function for module %s", activator_name, module->kobject.object_name);
			activator();
		}
	}
}

