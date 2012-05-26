#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>

#include <aul/parse.h>

#include <maxmodel/model.h>


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

static void compact(void ** elems, size_t length)
{
	size_t next = 0;
	for (size_t i = 0; i < length; i++)
	{
		if (elems[i] != NULL)
		{
			elems[next++] = elems[i];
		}
	}
}

static void * model_malloc(model_t * model, modeltype_t type, model_destroy_f destroy, size_t size)
{
	// Sanity check
	{
		if (unlikely(model == NULL || size < sizeof(modelhead_t)))
		{
			return NULL;
		}
	}

	if (unlikely(model->numids >= MODEL_MAX_IDS))
	{
		// No memory for new ID
		return NULL;
	}

	modelhead_t * head = malloc(size);
	if (likely(head != NULL))
	{
		model->malloc_size += size;

		memset(head, 0, size);
		model->id[head->id = model->numids++] = head;
		head->type = type;
		head->destroy = destroy;
	}

	return head;
}

static bool model_free(model_t * model, modelhead_t * self, const modelhead_t * target)
{
	if (self == NULL)
	{
		return false;
	}

	if (self->destroy(model, self, target))
	{
		model->id[self->id] = NULL;
		free(self);
		return true;
	}

	return false;
}

#define model_handlefree(elem) \
	if ((elem) != NULL && model_free(model, &(elem)->head, (isself)? NULL : target)) (elem) = NULL;

#define model_array_handlefree(elems, size) \
	for (size_t i = 0; i < (size); i++) model_handlefree((elems)[i]); compact((void **)elems, (size))



static bool model_model_destroy(model_t * model, modelhead_t * self, const modelhead_t * target)
{
	bool isself = (target == NULL)? true : self->id == target->id;

	model_array_handlefree(model->scripts, MODEL_MAX_SCRIPTS);

	if (isself)
	{
		meta_t * meta = NULL;
		model_foreach(meta, model->backings)
		{
			meta_destroy(meta);
		}
		memset(model->backings, 0, sizeof(meta_t *) * (MODEL_MAX_BACKINGS + MODEL_SENTINEL));
	}

	return isself;
}

static bool model_script_destroy(model_t * model, modelhead_t * self, const modelhead_t * target)
{
	model_script_t * script = (model_script_t *)self;
	bool isself = (target == NULL)? true : self->id == target->id;

	model_array_handlefree(script->links, MODEL_MAX_LINKS);
	model_array_handlefree(script->linkables, MODEL_MAX_LINKABLES);
	model_array_handlefree(script->modules, MODEL_MAX_MODULES);

	return isself;
}

static bool model_module_destroy(model_t * model, modelhead_t * self, const modelhead_t * target)
{
	model_module_t * module = (model_module_t *)self;
	bool isself = (target == NULL)? true : self->id == target->id;

	model_array_handlefree(module->configs, MODEL_MAX_CONFIGS);

	return isself;
}

static bool model_config_destroy(model_t * model, modelhead_t * self, const modelhead_t * target)
{
	return (target == NULL)? true : self->id == target->id;
}

static bool model_linkable_destroy(model_t * model, modelhead_t * self, const modelhead_t * target)
{
	model_linkable_t * linkable = (model_linkable_t *)self;
	bool isself = (target == NULL)? true : self->id == target->id;

	if (!isself)
	{
		if (self->type == model_rategroup && target->type == model_blockinst)
		{
			for (size_t i = 0; i < MODEL_MAX_RATEGROUPELEMS; i++)
			{
				const model_linkable_t * item = linkable->backing.rategroup->blocks[i];
				if (item != NULL && item->head.id == target->id)
				{
					linkable->backing.rategroup->blocks[i] = NULL;
				}
			}
			compact((void **)linkable->backing.rategroup->blocks, MODEL_MAX_RATEGROUPELEMS);
		}
	}

	return isself;
}

static bool model_link_destroy(model_t * model, modelhead_t * self, const modelhead_t * target)
{
	model_link_t * link = (model_link_t *)self;
	bool isself = (target == NULL)? true : self->id == target->id;

	if (!isself)
	{
		// If any of the linkables we are linking is being deleted, delete ourselves too
		isself |= link->out.linkable->head.id == target->id;
		isself |= link->in.linkable->head.id == target->id;
	}

	return isself;
}

#define traverse(model, func, t, cbs, items, ...) \
	({ \
		modelhead_t * item = NULL;								\
		model_foreach(item, (modelhead_t **)items)				\
		{														\
			func(model, t, cbs, (void *)item, ##__VA_ARGS__);	\
		}														\
	})

#define model_handlecallback(callback, model, object, ...) \
	({ \
		if (callback != NULL)					\
		{										\
			object->head.userdata = callback(object->head.userdata, model, object, ##__VA_ARGS__); \
		}										\
	})

static void model_model_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs);
static void model_script_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, model_script_t * script);
static void model_module_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, model_module_t * module, model_script_t * script);
static void model_config_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, model_config_t * config, model_script_t * script, model_module_t * module);
static void model_linkable_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, model_linkable_t * linkable, model_script_t * script);
static void model_link_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, model_link_t * link, model_script_t * script);

static void model_model_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs)
{
	switch (traversal)
	{
		case traversal_scripts_modules_configs_linkables_links:
		{
			traverse(model, model_script_analyse, traversal, cbs, model->scripts);
			break;
		}

		default: break;
	}
}

static void model_script_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, model_script_t * script)
{
	switch (traversal)
	{
		case traversal_scripts_modules_configs_linkables_links:
		{
			model_handlecallback(cbs->scripts, model, script);
			traverse(model, model_module_analyse, traversal, cbs, script->modules, script);
			traverse(model, model_linkable_analyse, traversal, cbs, script->linkables, script);
			traverse(model, model_link_analyse, traversal, cbs, script->links, script);
			break;
		}

		default: break;
	}
}

static void model_module_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, model_module_t * module, model_script_t * script)
{
	switch (traversal)
	{
		case traversal_scripts_modules_configs_linkables_links:
		{
			model_handlecallback(cbs->modules, model, module, script);
			traverse(model, model_config_analyse, traversal, cbs, module->configs, script, module);
			break;
		}

		default: break;
	}
}

static void model_config_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, model_config_t * config, model_script_t * script, model_module_t * module)
{
	switch (traversal)
	{
		case traversal_scripts_modules_configs_linkables_links:
		{
			model_handlecallback(cbs->configs, model, config, script, module);
			break;
		}

		default: break;
	}
}

static void model_linkable_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, model_linkable_t * linkable, model_script_t * script)
{
	void linkable_handlecallbacks()
	{
		model_handlecallback(cbs->linkables, model, linkable);
		switch (linkable->head.type)
		{
			case model_blockinst:	model_handlecallback(cbs->blockinsts, model, linkable, linkable->backing.blockinst);			break;
			case model_syscall:		model_handlecallback(cbs->syscalls, model, linkable, linkable->backing.syscall, script);		break;
			case model_rategroup:	model_handlecallback(cbs->rategroups, model, linkable, linkable->backing.rategroup, script);	break;
			default: break;
		}
	}

	switch (traversal)
	{
		case traversal_scripts_modules_configs_linkables_links:
		{
			linkable_handlecallbacks();
			break;
		}

		default: break;
	}
}

static void model_link_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, model_link_t * link, model_script_t * script)
{
	switch (traversal)
	{
		case traversal_scripts_modules_configs_linkables_links:
		{
			model_handlecallback(cbs->links, model, link, &link->out, &link->in);
			break;
		}

		default: break;
	}
}



model_t * model_new()
{
	model_t * model = malloc(sizeof(model_t));
	if (model == NULL)
	{
		return NULL;
	}

	memset(model, 0, sizeof(model_t));
	model->malloc_size += sizeof(model_t);

	modelhead_t * head = &model->head;



	model->id[head->id = model->numids++] = head;
	head->type = model_model;
	head->destroy = model_model_destroy;

	return model;
}

void * model_setuserdata(modelhead_t * head, void * newuserdata)
{
	// Sanity check
	{
		if (head == NULL)
		{
			return NULL;
		}
	}

	void * olduserdata = head->userdata;
	head->userdata = newuserdata;
	return olduserdata;
}

void model_clearalluserdata(model_t * model)
{
	// Sanity check
	{
		if (model == NULL)
		{
			return;
		}
	}

	for (size_t i = 0; i < model->numids; i++)
	{
		model->id[i]->userdata = NULL;
	}
}

void model_destroy(model_t * model, const modelhead_t * item)
{
	// Sanity check
	{
		if (model == NULL)
		{
			return;
		}
	}

	if (item == NULL)
	{
		item = model_object(model);
	}

	model_free(model, model_object(model), item);
}

void model_addmeta(model_t * model, const meta_t * meta, exception_t ** err)
{
	meta_t ** mem = nextfree(meta_t, model->backings, MODEL_MAX_BACKINGS);
	if (mem == NULL)
	{
		exception_set(err, ENOMEM, "Out of module backing memory! (MODEL_MAX_BACKINGS = %d)", MODEL_MAX_BACKINGS);
		return;
	}

	*mem = meta_copy(meta);
}

model_script_t * model_newscript(model_t * model, const char * path, exception_t ** err)
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
		model_foreach(item, model->scripts)
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

	strcpy(script->path, path);
	return script;
}

model_module_t * model_newmodule(model_t * model, model_script_t * script, meta_t * meta, exception_t ** err)
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
	meta_t * backing = NULL;
	{
		// See if module exists in script
		{
			model_module_t * item = NULL;
			model_foreach(item, script->modules)
			{
				if (strcmp(item->backing->path, meta->path) == 0)
				{
					module = item;
					backing = item->backing;
					break;
				}
			}
		}

		if (module == NULL)
		{
			// Module isn't registered with script, see if we can find the backing
			meta_t * item = NULL;
			model_foreach(item, model->backings)
			{
				if (strcmp(item->path, meta->path) == 0)
				{
					backing = item;
					break;
				}
			}
		}
	}

	if (backing == NULL)
	{
		meta_t ** mem = nextfree(meta_t, model->backings, MODEL_MAX_BACKINGS);
		if (mem == NULL)
		{
			exception_set(err, ENOMEM, "Out of module backing memory! (MODEL_MAX_BACKINGS = %d)", MODEL_MAX_BACKINGS);
			return NULL;
		}

		backing = *mem = meta_copy(meta);
		if (backing == NULL)
		{
			exception_set(err, ENOMEM, "Out of heap memory!");
			return NULL;
		}
	}

	if (module == NULL)
	{
		model_module_t ** mem = nextfree(model_module_t, script->modules, MODEL_MAX_MODULES);
		if (mem == NULL)
		{
			exception_set(err, ENOMEM, "Out of script module memory! (MODEL_MAX_MODULES = %d)", MODEL_MAX_MODULES);
			return NULL;
		}

		module = *mem = model_malloc(model, model_module, model_module_destroy, sizeof(model_module_t));
		if (module == NULL)
		{
			exception_set(err, ENOMEM, "Out of heap memory!");
			return NULL;
		}

		module->backing = backing;

		/* -- TODO - figure out the whole static block instance thingy
		// Create static block instance
		module->staticinst = model_newblockinst(model, module, script, "static", NULL, 0, err);
		if (module->staticinst == NULL || exception_check(err))
		{
			// An error happened!
			return NULL;
		}
		*/
	}
	else
	{
		// Module already exists, check the version and name to verify they are the same
		const char * path = NULL;
		const char * namea = NULL;				const char * nameb = NULL;
		const version_t * versiona = NULL;		const version_t * versionb = NULL;

		meta_getinfo(backing, &path, &namea, &versiona, NULL, NULL);
		meta_getinfo(meta, NULL, &nameb, &versionb, NULL, NULL);

		if (versiona != versionb)
		{
			string_t versiona_str = version_tostring(*versiona);
			string_t versionb_str = version_tostring(*versionb);
			exception_set(err, EINVAL, "Module version mismatch. Expected %s, got %s for module %s", versiona_str.string, versionb_str.string, path);
			return NULL;
		}

		if (strcmp(namea, nameb) != 0)
		{
			exception_set(err, EINVAL, "Module name mismatch. Expected %s, got %s for module %s", namea, nameb, path);
			return NULL;
		}
	}

	return module;
}

model_config_t * model_newconfig(model_t * model, model_module_t * module, const char * configname, const char * value, exception_t ** err)
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
			return NULL;
		}

		if (strlen(configname) >= MODEL_SIZE_NAME)
		{
			exception_set(err, ENOMEM, "Config name is too long! (MODEL_SIZE_NAME = %d)", MODEL_SIZE_NAME);
			return NULL;
		}

		if (strlen(value) >= MODEL_SIZE_VALUE)
		{
			exception_set(err, ENOMEM, "Config value is too long! (MODEL_SIZE_VALUE = %d)", MODEL_SIZE_VALUE);
			return NULL;
		}

		if (module->backing == NULL)
		{
			exception_set(err, EINVAL, "Module is unbound to meta object!");
			return NULL;
		}
	}

	// Check to see if we've already set this config value
	{
		const model_config_t * testconfig = NULL;
		model_foreach(testconfig, module->configs)
		{
			if (strcmp(testconfig->name, configname) == 0)
			{
				exception_set(err, EALREADY, "Config name %s already set", configname);
				return NULL;
			}
		}
	}

	const char * path = NULL;
	meta_getinfo(module->backing, &path, NULL, NULL, NULL, NULL);

	const meta_variable_t * variable = NULL;
	if (!meta_lookupconfig(module->backing, configname, &variable))
	{
		exception_set(err, EINVAL, "Module %s does not have config param %s", path, configname);
		return NULL;
	}

	char sig = 0;
	const char * desc = NULL;
	meta_getvariable(variable, NULL, &sig, &desc, NULL);

	// Check constraints
	constraint_t constraints = constraint_parse(desc, NULL);
	{
		string_t hint = string_blank();
		if (!constraint_check(&constraints, value, hint.string, string_available(&hint)))
		{
			exception_set(err, EINVAL, "Constraint check failed: %s", hint.string);
			return NULL;
		}
	}

	// Allocate script memory and add it to model
	model_config_t ** mem = nextfree(model_config_t, module->configs, MODEL_MAX_CONFIGS);
	if (mem == NULL)
	{
		exception_set(err, ENOMEM, "Out of comfig param memory! (MODEL_MAX_CONFIGPARAMS = %d)", MODEL_MAX_CONFIGS);
		return NULL;
	}

	model_config_t * configparam = *mem = model_malloc(model, model_config, model_config_destroy, sizeof(model_config_t) + strlen(value) + 1);
	if (configparam == NULL)
	{
		exception_set(err, ENOMEM, "Out of heap memory!");
		return NULL;
	}

	strcpy(configparam->name, configname);
	configparam->sig = sig;
	configparam->constraints = constraint_parse(desc, NULL);
	strcpy(configparam->value, value);

	return configparam;
}

model_linkable_t * model_newblockinst(model_t * model, model_module_t * module, model_script_t * script, const char * blockname, const char ** args, size_t args_length, exception_t ** err)
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

		if (args_length > 0 && args == NULL)
		{
			exception_set(err, EINVAL, "Bad args argument!");
			return NULL;
		}

		if (args_length > MODEL_MAX_ARGS)
		{
			exception_set(err, ENOMEM, "Block instance argument list too long! (MODEL_MAX_ARGS = %d)", MODEL_MAX_ARGS);
			return NULL;
		}

		if (strcmp(blockname, "static") == 0 && args_length > 0)
		{
			exception_set(err, EINVAL, "Bad args_length argument (static blocks have no arguments)!");
			return NULL;
		}

		if (module->backing == NULL)
		{
			exception_set(err, EINVAL, "Module is unbound to meta object!");
			return NULL;
		}
	}

	const char * path = NULL;
	const char * constructor_sig = "";
	meta_getinfo(module->backing, &path, NULL, NULL, NULL, NULL);

	// Verify the arguments
	{
		if (strcmp(blockname, "static") == 0)
		{
			// We are a static block, make sure that args == 0
			if (args_length != 0)
			{
				exception_set(err, EINVAL, "The static block instance for block %s cannot have arguments!", blockname);
				return NULL;
			}
		}
		else
		{
			const meta_block_t * block = NULL;
			if (!meta_lookupblock(module->backing, blockname, &block))
			{
				exception_set(err, EINVAL, "Module %s does not have block %s", path, blockname);
				return NULL;
			}

			meta_getblock(block, NULL, NULL, NULL, &constructor_sig, NULL, NULL);

			if (strlen(constructor_sig) != args_length)
			{
				exception_set(err, EINVAL, "Block %s constructor argument mismatch. Expected %zu argument(s) for signature %s, got %zu", blockname, strlen(constructor_sig), constructor_sig, args_length);
				return NULL;
			}
		}
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

	model_linkable_t * linkable = *mem = model_malloc(model, model_blockinst, model_linkable_destroy, sizeof(model_linkable_t) + sizeof(model_blockinst_t) + args_size);
	if (linkable == NULL)
	{
		exception_set(err, ENOMEM, "Out of heap memory!");
		return NULL;
	}

	model_blockinst_t * blockinst = (model_blockinst_t *)&linkable[1];
	linkable->backing.blockinst = blockinst;

	strcpy(blockinst->name, blockname);
	blockinst->module = module;
	strcpy(blockinst->args_sig, constructor_sig);

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

model_linkable_t * model_newrategroup(model_t * model, model_script_t * script, const char * groupname, double hertz, const model_linkable_t ** elems, size_t elems_length, exception_t ** err)
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

	model_linkable_t ** mem = nextfree(model_linkable_t, script->linkables, MODEL_MAX_LINKABLES);
	if (mem == NULL)
	{
		exception_set(err, ENOMEM, "Out of linkable instance memory! (MODEL_MAX_LINKABLES = %d)", MODEL_MAX_LINKABLES);
		return NULL;
	}

	model_linkable_t * linkable = *mem = model_malloc(model, model_rategroup, model_linkable_destroy, sizeof(model_linkable_t) + sizeof(model_rategroup_t));
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
		rategroup->blocks[i] = elems[i];
	}

	return linkable;
}

model_linkable_t * model_newsyscall(model_t * model, model_script_t * script, const char * funcname, const char * sig, const char * desc, exception_t ** err)
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

		if (strlen(sig) >= MODEL_SIZE_SIGNATURE)
		{
			exception_set(err, ENOMEM, "Syscall signature too long! (MODEL_SIZE_SIGNATURE = %d)", MODEL_SIZE_SIGNATURE);
			return NULL;
		}

		if (desc != NULL && strlen(desc) >= MODEL_SIZE_DESCRIPTION)
		{
			exception_set(err, ENOMEM, "Syscall description is too long! (MODEL_SIZE_SHORTDESCRIPTION = %d)", MODEL_SIZE_DESCRIPTION);
			return NULL;
		}
	}

	// Check to see if syscall has already been created in script
	{
		const model_linkable_t * testlinkable = NULL;
		model_foreach(testlinkable, script->linkables)
		{
			if (model_type(model_object(testlinkable)) == model_syscall && strcmp(testlinkable->backing.syscall->name, funcname) == 0)
			{
				exception_set(err, EALREADY, "Syscall %s already created!", funcname);
				return NULL;
			}
		}
	}

	// Allocate script memory and add it to model
	model_linkable_t ** mem = nextfree(model_linkable_t, script->linkables, MODEL_MAX_LINKABLES);
	if (mem == NULL)
	{
		exception_set(err, ENOMEM, "Out of linkable instance memory! (MODEL_MAX_LINKABLES = %d)", MODEL_MAX_LINKABLES);
		return NULL;
	}

	model_linkable_t * linkable = *mem = model_malloc(model, model_syscall, model_linkable_destroy, sizeof(model_linkable_t) + sizeof(model_syscall_t));
	if (linkable == NULL)
	{
		exception_set(err, ENOMEM, "Out of heap memory!");
		return NULL;
	}

	model_syscall_t * syscall = (model_syscall_t *)&linkable[1];
	linkable->backing.syscall = syscall;

	strcpy(syscall->name, funcname);
	strcpy(syscall->sig, sig);
	if (desc != NULL)
	{
		strcpy(syscall->description, desc);
	}

	return linkable;
}

model_link_t * model_newlink(model_t * model, model_script_t * script, model_linkable_t * out, const char * outname, model_linkable_t * in, const char * inname, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}

		if (model == NULL || script == NULL || out == NULL || in == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}

	// Parse out the symbol
	bool makesym(const model_linkable_t * linkable, const char * name, model_linksymbol_t * sym, exception_t ** err)
	{
		// Sanity check
		{
			if (exception_check(err))
			{
				return false;
			}

			if (linkable == NULL || name == NULL)
			{
				exception_set(err, EINVAL, "Bad arguments!");
				return false;
			}

			if (strlen(name) >= MODEL_SIZE_BLOCKIONAME)
			{
				exception_set(err, EINVAL, "Block io name (%s) too long! (MODEL_SIZE_BLOCKIONAME = %d)", name, MODEL_SIZE_BLOCKIONAME);
				return false;
			}
		}

		char base[MODEL_SIZE_BLOCKIONAME] = {0};

		strcpy(base, name);

		regex_t pattern = {0};
		if (regcomp(&pattern, "^(.+)\\[([0-9]+)\\]$", REG_EXTENDED) != 0)
		{
			exception_set(err, EFAULT, "Bad model_newlink -> makesym regex!");
			return false;
		}

		regmatch_t match[3];
		memset(match, 0, sizeof(regmatch_t) * 3);

		if (regexec(&pattern, base, 3, match, 0) == 0)
		{
			// Regex matched, pull base and index out
			base[match[1].rm_eo] = '\0';
			base[match[2].rm_eo] = '\0';
			sym->index = parse_int(&base[match[2].rm_so], NULL);
			sym->attrs.indexed = true;
		}

		regfree(&pattern);

		sym->linkable = linkable;
		strcpy(sym->name, base);

		return true;
	}

	model_linksymbol_t outsym, insym;
	memset(&outsym, 0, sizeof(model_linksymbol_t));
	memset(&insym, 0, sizeof(model_linksymbol_t));

	if (!makesym(out, outname, &outsym, err) || !makesym(in, inname, &insym, err))
	{
		// An error happened in makesym (exception was set in function), just return NULL
		return NULL;
	}

	// Verify that out_name and in_name are valid!
	switch (out->head.type)
	{
		case model_blockinst:
		{
			model_blockinst_t * blockinst = out->backing.blockinst;
			meta_t * backing = blockinst->module->backing;

			const meta_block_t * block = NULL;
			if (!meta_lookupblock(backing, blockinst->name, &block))
			{
				exception_set(err, EINVAL, "Could not lookup block %s", blockinst->name);
				return NULL;
			}

			if (!meta_lookupblockio(backing, block, outname, meta_output, NULL))
			{
				exception_set(err, EINVAL, "Block instance has no output named '%s'", outname);
				return NULL;
			}

			break;
		}

		case model_rategroup:
		{
			exception_set(err, EINVAL, "Rategroup has not output named '%s'", outname);
			return NULL;
		}

		case model_syscall:
		{
			if (strcmp(outsym.name, "a") != 0)
			{
				exception_set(err, EINVAL, "Syscall has no output named '%s' (Only options: 'a[0]' ... 'a[n]')", outname);
				return NULL;
			}

			break;
		}

		default:
		{
			exception_set(err, EINVAL, "Not a linkable output given!");
			return NULL;
		}
	}

	switch (in->head.type)
	{
		case model_blockinst:
		{
			model_blockinst_t * blockinst = in->backing.blockinst;
			meta_t * backing = blockinst->module->backing;

			const meta_block_t * block = NULL;
			if (!meta_lookupblock(backing, blockinst->name, &block))
			{
				exception_set(err, EINVAL, "Could not lookup block %s", blockinst->name);
				return NULL;
			}

			if (!meta_lookupblockio(backing, block, inname, meta_input, NULL))
			{
				exception_set(err, EINVAL, "Block instance has no input named '%s'", inname);
				return NULL;
			}

			break;
		}

		case model_rategroup:
		{
			if (strcmp(inname, "rate") != 0)
			{
				exception_set(err, EINVAL, "Rategroup has no input named '%s' (Only options: 'rate')", inname);
				return NULL;
			}

			break;
		}

		case model_syscall:
		{
			if (strcmp(inname, "r") != 0)
			{
				exception_set(err, EINVAL, "Syscall has no input named '%s' (Only options: 'r')", inname);
				return NULL;
			}

			break;
		}

		default:
		{
			exception_set(err, EINVAL, "Not a linkable input given!");
			return NULL;
		}
	}

	// Allocate script memory and add it to model
	model_link_t ** mem = nextfree(model_link_t, script->links, MODEL_MAX_LINKS);
	if (mem == NULL)
	{
		exception_set(err, ENOMEM, "Out of link memory! (MODEL_MAX_LINKS = %d)", MODEL_MAX_LINKS);
		return NULL;
	}

	model_link_t * link = *mem = model_malloc(model, model_link, model_link_destroy, sizeof(model_link_t));
	if (link == NULL)
	{
		exception_set(err, ENOMEM, "Out of heap memory!");
		return NULL;
	}

	link->out = outsym;
	link->in = insym;

	return link;
}

void model_analyse(model_t * model, modeltraversal_t traversal, const model_analysis_t * funcs)
{
	// Sanity check
	{
		if (model == NULL || funcs == NULL)
		{
			return;
		}
	}

	model_model_analyse(model, traversal, funcs);
}

bool model_lookupscript(const model_t * model, const char * path, const model_script_t ** script)
{
	// Sanity check
	{
		if (model == NULL || path == NULL)
		{
			return false;
		}
	}

	model_script_t * test = NULL;
	model_foreach(test, model->scripts)
	{
		if (strcmp(test->path, path) == 0)
		{
			if (script != NULL)		*script = test;
			return true;
		}
	}

	return false;
}

iterator_t model_scriptitr(const model_t * model)
{
	// Sanity check
	{
		if (model == NULL)
		{
			return iterator_none();
		}
	}

	const void * sitr_next(const void * object, void ** itrobject)
	{
		const model_t * model = object;
		model_script_t ** script = (model_script_t **)*itrobject;
		*itrobject = (*script == NULL)? (void *)&model->scripts[0] : (void *)&script[1];

		return *script;
	}

	return iterator_new("model_script", sitr_next, NULL, model, (void *)&model->scripts[0]);
}

bool model_scriptnext(iterator_t itr, const model_script_t ** script)
{
	const model_script_t * nextscript = iterator_next(itr, "model_script");
	if (nextscript != NULL)
	{
		if (script != NULL)		*script = nextscript;
		return true;
	}

	return false;
}

bool model_lookupmeta(const model_t * model, const char * path, const meta_t ** meta)
{
	// Sanity check
	{
		if (model == NULL || path == NULL)
		{
			return false;
		}
	}

	meta_t * test = NULL;
	model_foreach(test, model->backings)
	{
		const char * testpath = NULL;
		meta_getinfo(test, &testpath, NULL, NULL, NULL, NULL);

		if (strcmp(path, testpath) == 0)
		{
			if (meta != NULL)		*meta = test;
			return true;
		}
	}

	return false;
}

bool model_lookupmodule(const model_t * model, model_script_t * script, const char * path, model_module_t ** module)
{
	// Sanity check
	{
		if (model == NULL || script == NULL || path == NULL)
		{
			return false;
		}
	}

	model_module_t * test = NULL;
	model_foreach(test, script->modules)
	{
		const char * testpath = NULL;
		meta_getinfo(test->backing, &testpath, NULL, NULL, NULL, NULL);

		if (strcmp(path, testpath) == 0)
		{
			if (module != NULL)
			{
				*module = test;
			}

			return true;
		}
	}

	return false;
}

iterator_t model_moduleitr(const model_t * model, const model_script_t * script)
{
	// Sanity check
	{
		if (model == NULL || script == NULL)
		{
			return iterator_none();
		}
	}

	const void * mitr_next(const void * object, void ** itrobject)
	{
		const model_script_t * script = object;
		model_module_t ** module = (model_module_t **)*itrobject;
		*itrobject = (*module == NULL)? (void *)&script->modules[0] : (void *)&module[1];

		return *module;
	}

	return iterator_new("model_module", mitr_next, NULL, script, (void *)&script->modules[0]);
}

bool model_modulenext(iterator_t itr, const model_module_t ** module)
{
	const model_module_t * nextmodule = iterator_next(itr, "model_module");
	if (nextmodule != NULL)
	{
		if (module != NULL)		*module = nextmodule;
		return true;
	}

	return false;
}

bool model_lookupconfig(const model_t * model, const model_module_t * module, const char * name, const model_config_t ** config)
{
	// Sanity check
	{
		if (model == NULL || module == NULL || name == NULL)
		{
			return false;
		}
	}

	model_config_t * test = NULL;
	model_foreach(test, module->configs)
	{
		if (strcmp(test->name, name) == 0)
		{
			if (config != NULL)		*config = test;
			return true;
		}
	}

	return false;
}

iterator_t model_configitr(const model_t * model, const model_module_t * module)
{
	// Sanity check
	{
		if (model == NULL || module == NULL)
		{
			return iterator_none();
		}
	}

	const void * citr_next(const void * object, void ** itrobject)
	{
		const model_module_t * module = object;
		model_config_t ** config = (model_config_t **)*itrobject;
		*itrobject = (*config == NULL)? (void *)&module->configs[0] : (void *)&config[1];

		return *config;
	}

	return iterator_new("model_config", citr_next, NULL, module, (void *)&module->configs[0]);
}

bool model_confignext(iterator_t itr, const model_config_t ** config)
{
	const model_config_t * nextconfig = iterator_next(itr, "model_config");
	if (nextconfig != NULL)
	{
		if (config != NULL)		*config = nextconfig;
		return true;
	}

	return false;
}

iterator_t model_linkableitr(const model_t * model, const model_script_t * script)
{
	// Sanity check
	{
		if (model == NULL || script == NULL)
		{
			return iterator_none();
		}
	}

	const void * litr_next(const void * object, void ** itrobject)
	{
		const model_script_t * script = object;
		model_linkable_t ** linkable = (model_linkable_t **)*itrobject;
		*itrobject = (*linkable == NULL)? (void *)&script->linkables[0] : (void *)&linkable[1];

		return *linkable;
	}

	return iterator_new("model_linkable", litr_next, NULL, script, (void *)&script->linkables[0]);
}

bool model_linkablenext(iterator_t itr, const model_linkable_t ** linkable, modeltype_t * type)
{
	const model_linkable_t * nextlinkable = iterator_next(itr, "model_linkable");

	if (nextlinkable != NULL)
	{
		if (linkable != NULL)		*linkable = nextlinkable;
		if (type != NULL)			*type = model_type(model_object(nextlinkable));
		return true;
	}

	return false;
}






iterator_t model_linkitr(const model_t * model, const model_script_t * script, const model_linkable_t * linkable)
{
	// Sanity check
	{
		if (model == NULL || script == NULL || linkable == NULL)
		{
			return iterator_none();
		}
	}

	const void * litr_next(const void * object, void ** itrobject)
	{
		const model_linkable_t * linkable = object;

		while (true)
		{
			model_link_t ** lnk = (model_link_t **)*itrobject;
			*itrobject = (*lnk == NULL)? *lnk : (void *)&lnk[1];

			if (*lnk == NULL)
				return NULL;

			if ((*lnk)->out.linkable == linkable || (*lnk)->in.linkable == linkable)
				return *lnk;
		}
	}

	return iterator_new("model_link", litr_next, NULL, linkable, (void *)&script->links[0]);
}

bool model_linknext(iterator_t itr, const model_linksymbol_t ** out, const model_linksymbol_t ** in)
{
	const model_link_t * nextlink = iterator_next(itr, "model_link");
	if (nextlink != NULL)
	{
		if (out != NULL)		*out = &nextlink->out;
		if (in != NULL)			*in = &nextlink->in;
		return true;
	}

	return false;
}














void model_getscript(const model_script_t * script, const char ** path)
{
	// Sanity check
	{
		if (script == NULL)
		{
			return;
		}
	}

	if (path != NULL)		*path = script->path;
}

void model_getmodule(const model_module_t * module, const char ** path, const meta_t ** meta)
{
	// Sanity check
	{
		if (module == NULL)
		{
			return;
		}
	}

	meta_getinfo(module->backing, path, NULL, NULL, NULL, NULL);
	if (meta != NULL)		*meta = module->backing;
}

void model_getconfig(const model_config_t * config, const char ** name, char * sig, constraint_t * constraints, const char ** value)
{
	// Sanity check
	{
		if (config == NULL)
		{
			return;
		}
	}

	if (name != NULL)			*name = config->name;
	if (sig != NULL)			*sig = config->sig;
	if (constraints != NULL)	*constraints = config->constraints;
	if (value != NULL)			*value = config->value;
}

void model_getblockinst(const model_linkable_t * linkable, const char ** name, const model_module_t ** module, const char ** sig, const char * const ** args, size_t * argslen)
{
	// Sanity check
	{
		if (linkable == NULL || model_type(model_object(linkable)) != model_blockinst)
		{
			return;
		}
	}

	const model_blockinst_t * blockinst = linkable->backing.blockinst;
	if (name != NULL)			*name = blockinst->name;
	if (module != NULL)			*module = blockinst->module;
	if (sig != NULL)			*sig = blockinst->args_sig;
	if (args != NULL)			*args = (const char * const *)blockinst->args;
	if (argslen != NULL)		*argslen = strlen(blockinst->args_sig);
}

void model_getsyscall(const model_linkable_t * linkable, const char ** name, const char ** sig, const char ** desc)
{
	// Sanity check
	{
		if (linkable == NULL || model_type(model_object(linkable)) != model_syscall)
		{
			return;
		}
	}

	const model_syscall_t * syscall = linkable->backing.syscall;
	if (name != NULL)			*name = syscall->name;
	if (sig != NULL)			*sig = syscall->sig;
	if (desc != NULL)			*desc = syscall->description;
}

void model_getrategroup(const model_linkable_t * linkable, const char ** name, double * hertz)
{
	// Sanity check
	{
		if (linkable == NULL || model_type(model_object(linkable)) != model_rategroup)
		{
			return;
		}
	}

	const model_rategroup_t * rategroup = linkable->backing.rategroup;
	if (name != NULL)			*name = rategroup->name;
	if (hertz != NULL)			*hertz = rategroup->hertz;
}

void model_getlinksymbol(const model_linksymbol_t * symbol, const model_linkable_t ** linkable, const char ** name, bool * hasindex, size_t * index)
{
	// Sanity check
	{
		if (symbol == NULL)
		{
			return;
		}
	}

	if (linkable != NULL)		*linkable = symbol->linkable;
	if (name != NULL)			*name = symbol->name;
	if (hasindex != NULL)		*hasindex = symbol->attrs.indexed;
	if (index != NULL)			*index = symbol->index;
}
