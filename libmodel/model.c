#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <model.h>


#define model_foreach(item, items, maxsize) \
	for (size_t __i = 0; ((item) = (items)[__i]) != NULL && __i < (maxsize); __i++)

#define nextfree(type, items, maxsize)	(type **)__nextfree((void **)(items), (maxsize))
static inline void ** __nextfree(void ** items, size_t maxsize)
{
	for (size_t i = 0; i < maxsize; i++)
	{
		if (items[i] == NULL)
		{
			return &items[i];
		}
	}

	return NULL;
}


static void * model_malloc(model_t * model, modeltype_t type, model_destroy_f destroy, size_t bytes)
{
	if (bytes < sizeof(modelhead_t))
	{
		return NULL;
	}

	modelhead_t * head = malloc(bytes);
	memset(head, 0, bytes);

	model->id[head->id = model->numids++] = head;
	head->type = type;
	head->destroy = destroy;

	return head;
}


model_t * model_new()
{
	model_t * model = malloc(sizeof(model_t));
	memset(model, 0, sizeof(model_t));

	return model;
}

void model_free(model_t * model)
{
	// Destroy the scripts
	model_script_t * script = NULL;
	model_foreach (script, model->scripts, MODEL_MAX_SCRIPTS)
	{
		script->head.destroy(model, script);
	}

	// Destroy the modules
	model_module_t * module = NULL;
	model_foreach(module, model->modules, MODEL_MAX_MODULES * MODEL_MAX_SCRIPTS)
	{
		module->head.destroy(model, module);
	}

	// Free the model
	free(model);
}


static void model_script_destroy(model_t * model, model_script_t * script)
{
	// TODO - finish me!
}

static void model_module_destroy(model_t * model, model_script_t * script)
{
	// TODO - finish me!
}

model_script_t * model_script_newscript_withpath(model_t * model, const char * path, exception_t ** err)
{
	// Sanity check
	{
		if (model == NULL || path == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}

		if (strlen(path) >= MODEL_SIZE_PATH)
		{
			// Path is too large and will truncate
			exception_set(err, ENOMEM, "Path is too long! (MODEL_SIZE_PATH = %d)", MODEL_SIZE_PATH);
			return NULL;
		}
	}

	// If script already exists, throw error
	{
		model_script_t * item = NULL;
		model_foreach(item, model->scripts, MODEL_MAX_SCRIPTS)
		{
			if (strcmp(item->path, path) == 0)
			{
				exception_set(err, EEXIST, "Script file %s already exists!", path);
				return NULL;
			}
		}
	}

	// Allocate script memory and add it to model
	model_script_t ** mem = nextfree(model_script_t, model->scripts, MODEL_MAX_SCRIPTS);
	if (mem == NULL)
	{
		exception_set(err, ENOMEM, "Out of script memory! (MODEL_MAX_SCRIPTS = %d)", MODEL_MAX_SCRIPTS);
		return NULL;
	}

	model_script_t * script = *mem = model_malloc(model, model_script, model_script_destroy, sizeof(model_script_t));
	if (script == NULL)
	{
		exception_set(err, ENOMEM, "Out of heap memory!");
		return NULL;
	}

	strncpy(script->path, path, MODEL_SIZE_PATH-1);
	return script;
}

model_module_t * model_script_addmodule_frommeta(model_t * model, model_script_t * script, meta_t * meta, exception_t ** err)
{
	// Sanity check
	{
		if (model == NULL || script == NULL || meta == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}

		if (meta->path[0] == '\0')
		{
			exception_set(err, EINVAL, "Bad meta argument!");
			return NULL;
		}

		if (strlen(meta->path) >= MODEL_SIZE_PATH)
		{
			// Path is too large and will truncate
			exception_set(err, ENOMEM, "Path is too long! (MODEL_SIZE_PATH = %d)", MODEL_SIZE_PATH);
			return NULL;
		}
	}

	model_module_t * module = NULL;
	{
		// See if module exists in script
		model_module_t * item = NULL;
		model_foreach(item, script->modules, MODEL_MAX_MODULES)
		{
			if (strcmp(item->meta->path, meta->path) == 0)
			{
				module = item;
			}
		}

		if (module == NULL)
		{
			// Module isn't registered with script, see if it's in the model
			model_foreach(item, model->modules, MODEL_MAX_MODULES * MODEL_MAX_SCRIPTS)
			{
				if (strcmp(item->meta->path, meta->path) == 0)
				{
					// Module exists in model, register it with script
					model_module_t ** mem = nextfree(model_module_t, script->modules, MODEL_MAX_MODULES);
					if (mem == NULL)
					{
						exception_set(err, ENOMEM, "Out of module memory! (MODEL_MAX_MODULES)");
						return NULL;
					}

					module = *mem = item;
					break;
				}
			}
		}
	}

	if (module == NULL)
	{
		model_module_t ** mem = nextfree(model_module_t, script->modules, MODEL_MAX_MODULES);
		if (mem == NULL)
		{
			exception_set(err, ENOMEM, "Out of module memory! (MODEL_MAX_MODULES)");
			return NULL;
		}

		module = *mem = model_malloc(model, model_module, model_module_destroy, sizeof(model_module_t));
		if (module == NULL)
		{
			exception_set(err, ENOMEM, "Out of heap memory!");
			return NULL;
		}

		// Copy the meta object over
		module->meta = meta_copy(meta);
		//strncpy(module->path, module->meta->path, MODEL_SIZE_PATH);
		//module->version = module->meta->version;
	}
	else
	{
		// Module already exists, check the version to verify they are the same
		if (module->meta->version != meta->version)
		{
			string_t meta_version = version_tostring(meta->version->version);
			string_t module_version = version_tostring(module->meta->version->version);
			exception_set(err, EINVAL, "Module version mismatch. Expected %s, got %s for module %s", module_version.string, meta_version.string, module->meta->path);
			return NULL;
		}
	}

	return module;
}

model_blockinst_t * model_module_newblockinst(model_t * model, model_module_t * module, const char * blockname, exception_t ** err)
{
	// Sanity check
	{
		if (model == NULL || module == NULL || blockname == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}

		if (strlen(blockname) >= MODEL_SIZE_BLOCKNAME)
		{
			// Path is too large and will truncate
			exception_set(err, ENOMEM, "Block name is too long! (MODEL_SIZE_BLOCKNAME = %d)", MODEL_SIZE_BLOCKNAME);
			return NULL;
		}
	}
/*
	char constructor_sig[META_SIZE_SIGNATURE];
	size_t ios_length = 0;

	if (!meta_getblock(blockname, constructor_sig, &ios_length))
	{
		exception_set(err, EINVAL, "Module %s does not have block %s", module->meta->path, blockname);
		return NULL;
	}
	*/

	return NULL;
}
