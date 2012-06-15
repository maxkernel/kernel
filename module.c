#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dlfcn.h>

#include <kernel.h>
#include <kernel-priv.h>


extern list_t modules;

static ssize_t module_desc(const kobject_t * object, char * buffer, size_t length)
{
	const module_t * module = (const module_t *)object;
	return meta_yamlinfo(module_meta(module), buffer, length);
}

static void module_destroy(kobject_t * object)
{
	module_t * module = (module_t *)object;

	// Destroy the configs
	{
		list_t * pos = NULL, * n = NULL;
		list_foreach_safe(pos, n, &module->configs)
		{
			config_t * config = list_entry(pos, config_t, module_list);
			list_remove(pos);
			kobj_destroy(kobj_cast(config));
		}
	}

	// Destroy all syscalls
	{
		list_t * pos = NULL, * n = NULL;
		list_foreach_safe(pos, n, &module->syscalls)
		{
			syscall_t * syscall = list_entry(pos, syscall_t, module_list);
			list_remove(pos);
			kobj_destroy(kobj_cast(syscall));
		}
	}

	// Destroy all blocks
	{
		list_t * pos = NULL, * n = NULL;
		list_foreach_safe(pos, n, &module->blocks)
		{
			block_t * block = list_entry(pos, block_t, module_list);
			list_remove(pos);
			kobj_destroy(kobj_cast(block));
		}
	}

	//remove from modules list
	list_remove(&module->global_list);

	// Call destroyer
	{
		meta_destroyer destroyer = NULL;
		meta_getactivators(module_meta(module), NULL, &destroyer, NULL, NULL);

		if (destroyer != NULL)
		{
			LOGK(LOG_DEBUG, "Calling destroy function for module %s", kobj_objectname(kobj_cast(module)));
			destroyer();
		}
	}

	// Destroy the meta backing object
	meta_destroy(module->meta);

	//do not dlclose module because that address space might still be in use
	// TODO - we can check that and unload if not in use!
}

module_t * module_lookup(const char * name)
{
	// Sanity check
	{
		if unlikely(name == NULL)
		{
			return NULL;
		}
	}

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
		meta_getinfo(module_meta(tester), &testerpath, NULL, NULL, NULL, NULL);

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
		iterator_t ditr = meta_itrdependency(meta);
		{
			const meta_dependency_t * dependency = NULL;
			while (meta_nextdependency(ditr, &dependency))
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

				module_t * depmodule = module_load(depmeta, lookup, err);
				if (depmodule == NULL || exception_check(err))
				{
					return NULL;
				}

				meta_destroy(depmeta);
			}
		}
		iterator_free(ditr);
	}

	// Try loading it
	if (!meta_loadmodule(meta, err))
	{
		return NULL;
	}

	module_t * module = kobj_new("Module", name, module_desc, module_destroy, sizeof(module_t));
	module->meta = meta_copy(meta);
	list_init(&module->syscalls);
	list_init(&module->configs);
	list_init(&module->blocks);
	list_init(&module->blockinsts);

	// Create configs
	{
		iterator_t citr = meta_itrconfig(meta);
		{
			const meta_variable_t * variable = NULL;
			while (meta_nextconfig(citr, &variable))
			{
				config_t * config = config_new(meta, variable, err);
				if (config == NULL || exception_check(err))
				{
					return NULL;
				}

				kobj_makechild(kobj_cast(module), kobj_cast(config));
				list_add(&module->configs, &config->module_list);
			}
		}
		iterator_free(citr);
	}

	// Create syscalls
	{
		iterator_t sitr = meta_itrsyscall(meta);
		{
			const meta_callback_t * callback = NULL;
			while(meta_nextsyscall(sitr, &callback))
			{
				const char * name = NULL, * sig = NULL, * desc = NULL;
				syscall_f func = NULL;
				meta_getcallback(callback, &name, &sig, &desc, &func);

				syscall_t * syscall = syscall_new(name, sig, func, desc, err);
				if (syscall == NULL || exception_check(err))
				{
					return NULL;
				}

				kobj_makechild(kobj_cast(module), kobj_cast(syscall));
				list_add(&module->syscalls, &syscall->module_list);
			}
		}
		iterator_free(sitr);
	}

	// Create blocks
	{
		const meta_block_t * block = NULL;
		iterator_t bitr = meta_itrblock(meta);
		{
			while (meta_nextblock(bitr, &block))
			{
				block_t * blk = block_new(module, block, err);
				if (blk == NULL || exception_check(err))
				{
					return NULL;
				}

				kobj_makechild(kobj_cast(module), kobj_cast(blk));
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
		if unlikely(module == NULL)
		{
			LOGK(LOG_ERR, "Module is NULL (module_init)");
			return;
		}
	}

	// Call initializer function
	{
		meta_initializer initializer = NULL;
		meta_getactivators(module_meta(module), &initializer, NULL, NULL, NULL);

		if (initializer != NULL)
		{
			LOGK(LOG_DEBUG, "Calling initializer function for module %s", kobj_objectname(kobj_cast(module)));

			if (!initializer())
			{
				LOGK(LOG_FATAL, "Module initialization failed (%s).", kobj_objectname(kobj_cast(module)));
				// Will exit
			}
		}
	}
}

void module_activate(const module_t * module, moduleact_t act)
{
	// Sanity check
	{
		if unlikely(module == NULL)
		{
			LOGK(LOG_ERR, "Bad arguments!");
			return;
		}
	}

	// Call activator function
	{
		meta_activator activator = NULL;
		const char * activator_name = "(unknown)";

		switch (act)
		{
			case act_preact:
				meta_getactivators(module_meta(module), NULL, NULL, &activator, NULL);
				activator_name = "preactivator";
				break;

			case act_postact:
				meta_getactivators(module_meta(module), NULL, NULL, NULL, &activator);
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

