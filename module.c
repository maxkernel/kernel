#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dlfcn.h>

#include <kernel.h>
#include <kernel-priv.h>


extern list_t modules;

static char * module_info(void * object)
{
	unused(object);

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

bool module_exists(const char * name)
{
	return module_lookup(name) != NULL;
}

module_t * module_load(model_t * model, meta_t * meta, metalookup_f lookup, exception_t ** err)
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

				// Get the dependency meta
				meta_t * depmeta = lookup(depname);
				if (depmeta == NULL)
				{
					exception_set(err, ENOENT, "Module dependency resolve failed for %s", depname);
					return NULL;
				}

				// Now add it to the model
				// **This will add a copy of the meta, so we must free it and re-resolve it**
				if (!model_addmeta(model, depmeta, err))
				{
					return NULL;
				}

				const char * deppath = NULL;
				meta_getinfo(depmeta, &deppath, NULL, NULL, NULL, NULL);

				const meta_t * newdepmeta = NULL;
				if (!model_lookupmeta(model, deppath, &newdepmeta))
				{
					exception_set(err, EFAULT, "Could not re-resolve the dependency meta object for %s", depname);
					return NULL;
				}

				// Now set the dependency meta object to the model meta object
				meta_destroy(depmeta);
				depmeta = (meta_t *)newdepmeta;

				module_t * depmodule = module_load(model, depmeta, lookup, err);
				if (depmodule == NULL || exception_check(err))
				{
					return NULL;
				}
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
}

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

void module_activate(const module_t * module, moduleact_t act)
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

