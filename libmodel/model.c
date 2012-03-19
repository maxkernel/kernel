#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <model.h>



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


static void * model_malloc(model_t * model, modeltype_t type, model_destroy_f destroy, model_analyse_f analyse, size_t size)
{
	// Sanity check
	{
		if (model == NULL || size < sizeof(modelhead_t))
		{
			return NULL;
		}
	}


	modelhead_t * head = malloc(size);
	if (LIKELY(head != NULL))
	{
		model->malloc_size += size;

		memset(head, 0, size);
		model->id[head->id = model->numids++] = head;
		head->type = type;
		head->destroy = destroy;
		head->analyse = analyse;
	}

	return head;
}


model_t * model_new()
{
	model_t * model = malloc(sizeof(model_t));
	memset(model, 0, sizeof(model_t));

	return model;
}

void * model_setuserdata(modelhead_t * head, void * newuserdata)
{
	void * olduserdata = head->userdata;
	head->userdata = newuserdata;
	return olduserdata;
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

string_t model_getconstraint(const char * desc)
{
	// TODO - FINISH ME!
	return string_blank();
}

string_t model_getbase(const char * ioname)
{
	// TODO - finish me!
	return string_blank();
}

string_t model_getsubscript(const char * ioname)
{
	// TODO - finish me!
	return string_blank();
}


static void model_script_destroy(model_t * model, model_script_t * script)
{
	// TODO - finish me!
}

static void model_module_destroy(model_t * model, model_module_t * module)
{
	// TODO - finish me!
}

static void model_configparam_destroy(model_t model, model_configparam_t * configparam)
{
	// TODO - finish me!
}

static void model_linkable_destroy(model_t model, model_linkable_t * linkable)
{
	// TODO - finish me!
}

static void model_link_destroy(model_t model, model_link_t * link)
{
	// TODO - finish me!
}



static void model_script_analyse(model_t * model, model_analysis_t * cbs, model_script_t * script)
{
	// TODO - finish me!
}

static void model_module_analyse(model_t * model, model_analysis_t * cbs, model_module_t * module)
{
	// TODO - finish me!
}

static void model_configparam_analyse(model_t model, model_analysis_t * cbs, model_configparam_t * configparam)
{
	// TODO - finish me!
}

static void model_linkable_analyse(model_t model, model_analysis_t * cbs, model_linkable_t * linkable)
{
	// TODO - finish me!
}

static void model_link_analyse(model_t model, model_analysis_t * cbs, model_link_t * link)
{
	// TODO - finish me!
}




model_script_t * model_script_new(model_t * model, const char * path, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}

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

	model_script_t * script = *mem = model_malloc(model, model_script, model_script_destroy, model_script_analyse, sizeof(model_script_t));
	if (script == NULL)
	{
		exception_set(err, ENOMEM, "Out of heap memory!");
		return NULL;
	}

	strcpy(script->path, path);
	return script;
}

model_module_t * model_module_add(model_t * model, model_script_t * script, meta_t * meta, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}

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
		model_module_t ** mem1 = nextfree(model_module_t, script->modules, MODEL_MAX_MODULES);
		if (mem1 == NULL)
		{
			exception_set(err, ENOMEM, "Out of script module memory! (MODEL_MAX_MODULES = %d)", MODEL_MAX_MODULES);
			return NULL;
		}

		model_module_t ** mem2 = nextfree(model_module_t, model->modules, MODEL_MAX_MODULES * MODEL_MAX_SCRIPTS);
		if (mem2 == NULL)
		{
			exception_set(err, ENOMEM, "Out of model module memory! (MODEL_MAX_MODULES * MODEL_MAX_SCRIPTS = %d)", MODEL_MAX_MODULES * MODEL_MAX_SCRIPTS);
			return NULL;
		}

		module = *mem1 = *mem2 = model_malloc(model, model_module, model_module_destroy, model_module_analyse, sizeof(model_module_t));
		if (module == NULL)
		{
			exception_set(err, ENOMEM, "Out of heap memory!");
			return NULL;
		}

		// Copy the meta object over
		module->meta = meta_copy(meta);
		strcpy(module->path, module->meta->path);
		strcpy(module->name, module->meta->name->name);
		module->version = module->meta->version->version;
	}
	else
	{
		// Module already exists, check the version and name to verify they are the same
		if (module->meta->version != meta->version)
		{
			string_t meta_version = version_tostring(meta->version->version);
			string_t module_version = version_tostring(module->meta->version->version);
			exception_set(err, EINVAL, "Module version mismatch. Expected %s, got %s for module %s", module_version.string, meta_version.string, module->meta->path);
			return NULL;
		}

		if (strcmp(module->meta->name->name, meta->name->name) != 0)
		{
			exception_set(err, EINVAL, "Module name mismatch. Expected %s, got %s for module %s", module->meta->name->name, meta->name->name, module->meta->path);
		}
	}

	return module;
}

bool model_module_setconfig(model_t * model, model_module_t * module, const char * configname, const char * value, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}

		if (model == NULL || module == NULL || configname == NULL || value == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}

		if (strlen(configname) >= MODEL_SIZE_NAME)
		{
			exception_set(err, ENOMEM, "Config name is too long! (MODEL_SIZE_NAME = %d)", MODEL_SIZE_NAME);
			return false;
		}

		if (module->meta == NULL)
		{
			exception_set(err, EINVAL, "Module is unbound to meta object!");
			return false;
		}
	}

	char sig;
	const char * desc;

	if (!meta_getconfigparam(module->meta, configname, &sig, &desc))
	{
		exception_set(err, EINVAL, "Module %s does not have config param %s", module->meta->path, configname);
		return false;
	}

	string_t constraint = model_getconstraint(desc);
	if (constraint.length >= MODEL_SIZE_CONSTRAINT)
	{
		exception_set(err, EINVAL, "Constraint syntax too long for config param %s in module %s", configname, module->meta->path);
		return false;
	}

	// Allocate script memory and add it to model
	model_configparam_t ** mem = nextfree(model_configparam_t, module->configparams, MODEL_MAX_CONFIGPARAMS);
	if (mem == NULL)
	{
		exception_set(err, ENOMEM, "Out of comfig param memory! (MODEL_MAX_CONFIGPARAMS = %d)", MODEL_MAX_CONFIGPARAMS);
		return false;
	}

	model_configparam_t * configparam = *mem = model_malloc(model, model_configparam, model_configparam_destroy, model_configparam_analyse, sizeof(model_configparam_t) + strlen(value) + 1);
	if (configparam == NULL)
	{
		exception_set(err, ENOMEM, "Out of heap memory!");
		return false;
	}

	strcpy(configparam->name, configname);
	configparam->sig = sig;
	strcpy(configparam->constraint, constraint.string);
	strcpy(configparam->value, value);

	return configparam;
}

model_linkable_t * model_blockinst_new(model_t * model, model_module_t * module, model_script_t * script, const char * blockname, const char ** args, size_t args_length, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}

		if (model == NULL || module == NULL || script == NULL || blockname == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}

		if (strlen(blockname) >= MODEL_SIZE_NAME)
		{
			exception_set(err, ENOMEM, "Block name is too long! (MODEL_SIZE_NAME = %d)", MODEL_SIZE_NAME);
			return NULL;
		}

		if (args_length > MODEL_MAX_ARGS)
		{
			exception_set(err, ENOMEM, "Block instance argument list too long! (MODEL_MAX_ARGS = %d)", MODEL_MAX_ARGS);
			return NULL;
		}

		if (module->meta == NULL)
		{
			exception_set(err, EINVAL, "Module is unbound to meta object!");
			return NULL;
		}
	}

	const char * constructor_sig = NULL;
	if (!meta_getblock(module->meta, blockname, &constructor_sig, NULL, NULL))
	{
		exception_set(err, EINVAL, "Module %s does not have block %s", module->meta->path, blockname);
		return NULL;
	}

	if (strlen(constructor_sig) != args_length)
	{
		exception_set(err, EINVAL, "Block %s constructor argument mismatch. Expected %zu argument(s) for signature %s, got %zu", blockname, strlen(constructor_sig), constructor_sig, args_length);
		return NULL;
	}

	size_t args_size = 0;
	for (size_t i = 0; i < args_length; i++)
	{
		args_size += strlen(args[i]) + 1;
	}

	// Allocate script memory and add it to model
	model_linkable_t ** mem = nextfree(model_linkable_t, script->linkables, MODEL_MAX_LINKABLES);
	if (mem == NULL)
	{
		exception_set(err, ENOMEM, "Out of linkable instance memory! (MODEL_MAX_LINKABLES = %d)", MODEL_MAX_LINKABLES);
		return NULL;
	}

	model_linkable_t * linkable = *mem = model_malloc(model, model_blockinst, model_linkable_destroy, model_link_analyse, sizeof(model_linkable_t) + sizeof(model_blockinst_t) + args_size);
	if (linkable == NULL)
	{
		exception_set(err, ENOMEM, "Out of heap memory!");
		return NULL;
	}

	model_blockinst_t * blockinst = (model_blockinst_t *)&linkable[1];
	linkable->backing.blockinst = blockinst;

	strcpy(blockinst->name, blockname);
	blockinst->module = module;
	char * buffer  = (char *)&blockinst[1];
	for (size_t i = 0; i < args_length; i++)
	{
		size_t size = strlen(args[i]) + 1;
		memcpy(buffer, args[i], size);

		blockinst->args[i] = buffer;
		buffer += size;
	}

	return linkable;
}

model_linkable_t * model_rategroup_new(model_t * model, model_script_t * script, const char * groupname, double hertz, const model_linkable_t ** elems, size_t elems_length, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}

		if (model == NULL || script == NULL || groupname == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}

		if (strlen(groupname) >= MODEL_SIZE_NAME)
		{
			exception_set(err, ENOMEM, "Rategroup name is too long! (MODEL_SIZE_NAME = %d)", MODEL_SIZE_NAME);
			return NULL;
		}

		if (elems_length > MODEL_MAX_RATEGROUPELEMS)
		{
			exception_set(err, ENOMEM, "Block instance argument list too long! (MODEL_MAX_RATEGROUPELEMS = %d)", MODEL_MAX_RATEGROUPELEMS);
			return NULL;
		}

		if (hertz <= 0)
		{
			exception_set(err, EINVAL, "Invalid update frequency! (%f)", hertz);
			return NULL;
		}
	}

	for (size_t i = 0; i < elems_length; i++)
	{
		if (elems[i]->head.type != model_blockinst)
		{
			exception_set(err, EINVAL, "Element #%d to rategroup %s is not a block instance!", i+1, groupname);
			return NULL;
		}
	}

	// Allocate script memory and add it to model
	model_linkable_t ** mem = nextfree(model_linkable_t, script->linkables, MODEL_MAX_LINKABLES);
	if (mem == NULL)
	{
		exception_set(err, ENOMEM, "Out of linkable instance memory! (MODEL_MAX_LINKABLES = %d)", MODEL_MAX_LINKABLES);
		return NULL;
	}

	model_linkable_t * linkable = *mem = model_malloc(model, model_rategroup, model_linkable_destroy, model_linkable_analyse, sizeof(model_linkable_t) + sizeof(model_rategroup_t));
	if (linkable == NULL)
	{
		exception_set(err, ENOMEM, "Out of heap memory!");
		return NULL;
	}

	model_rategroup_t * rategroup = (model_rategroup_t *)&linkable[1];
	linkable->backing.rategroup = rategroup;

	strcpy(rategroup->name, groupname);
	rategroup->hertz = hertz;
	for (size_t i = 0; i < elems_length; i++)
	{
		rategroup->blocks[i] = elems[i]->backing.blockinst;
	}

	return linkable;
}

model_linkable_t * model_syscall_new(model_t * model, model_script_t * script, const char * funcname, const char * desc, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}

		if (model == NULL || script == NULL || funcname == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}

		if (strlen(funcname) >= MODEL_SIZE_NAME)
		{
			exception_set(err, ENOMEM, "Syscall name is too long! (MODEL_SIZE_NAME = %d)", MODEL_SIZE_NAME);
			return NULL;
		}

		if (desc != NULL && strlen(desc) >= MODEL_SIZE_SHORTDESCRIPTION)
		{
			exception_set(err, ENOMEM, "Syscall description is too long! (MODEL_SIZE_SHORTDESCRIPTION = %d)", MODEL_SIZE_SHORTDESCRIPTION);
			return NULL;
		}
	}

	// Allocate script memory and add it to model
	model_linkable_t ** mem = nextfree(model_linkable_t, script->linkables, MODEL_MAX_LINKABLES);
	if (mem == NULL)
	{
		exception_set(err, ENOMEM, "Out of linkable instance memory! (MODEL_MAX_LINKABLES = %d)", MODEL_MAX_LINKABLES);
		return NULL;
	}

	model_linkable_t * linkable = *mem = model_malloc(model, model_syscall, model_linkable_destroy, model_linkable_analyse, sizeof(model_linkable_t) + sizeof(model_syscall_t));
	if (linkable == NULL)
	{
		exception_set(err, ENOMEM, "Out of heap memory!");
		return NULL;
	}

	model_syscall_t * syscall = (model_syscall_t *)&linkable[1];
	linkable->backing.syscall = syscall;

	strcpy(syscall->name, funcname);
	if (desc != NULL)
	{
		strcpy(syscall->description, desc);
	}

	return linkable;
}

model_link_t * model_link_new(model_t * model, model_script_t * script, model_linkable_t * outinst, const char * outname, model_linkable_t * ininst, const char * inname, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}

		if (model == NULL || script == NULL || outinst == NULL || ininst == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}

		if (strlen(outname) >= MODEL_SIZE_BLOCKIONAME)
		{
			exception_set(err, ENOMEM, "Block output name %s too long! (MODEL_SIZE_BLOCKIONAME = %d)", outname, MODEL_SIZE_BLOCKIONAME);
			return NULL;
		}

		if (strlen(inname) >= MODEL_SIZE_BLOCKIONAME)
		{
			exception_set(err, ENOMEM, "Block input name %s too long! (MODEL_SIZE_BLOCKIONAME = %d)", inname, MODEL_SIZE_BLOCKIONAME);
			return NULL;
		}
	}

	// TODO (IMPORTANT) verify that out_name and in_name are valid!

	// Allocate script memory and add it to model
	model_link_t ** mem = nextfree(model_link_t, script->links, MODEL_MAX_LINKS);
	if (mem == NULL)
	{
		exception_set(err, ENOMEM, "Out of link memory! (MODEL_MAX_LINKS = %d)", MODEL_MAX_LINKS);
		return NULL;
	}

	model_link_t * link = *mem = model_malloc(model, model_link, model_link_destroy, model_link_analyse, sizeof(model_link_t));
	if (link == NULL)
	{
		exception_set(err, ENOMEM, "Out of heap memory!");
		return NULL;
	}

	strcpy(link->out_name, outname);
	link->out = outinst;
	strcpy(link->in_name, inname);
	link->in = ininst;

	return link;
}
