#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>

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

static void * model_malloc(model_t * model, modeltype_t type, model_destroy_f destroy, model_analyse_f analyse, size_t size)
{
	// Sanity check
	{
		if (UNLIKELY(model == NULL || size < sizeof(modelhead_t)))
		{
			return NULL;
		}
	}

	if (UNLIKELY(model->numids >= MODEL_MAX_IDS))
	{
		// No memory for new ID
		return NULL;
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

static bool model_free(model_t * model, modelhead_t * self, modelhead_t * target)
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



static bool model_model_destroy(model_t * model, modelhead_t * self, modelhead_t * target)
{
	bool isself = (target == NULL)? true : self->id == target->id;

	model_array_handlefree(model->scripts, MODEL_MAX_SCRIPTS);

	meta_t * meta = NULL;
	model_foreach(meta, model->backings, MODEL_MAX_BACKINGS)
	{
		meta_destroy(meta);
	}
	memset(model->backings, 0, sizeof(void *) * MODEL_MAX_BACKINGS);

	return isself;
}

static bool model_script_destroy(model_t * model, modelhead_t * self, modelhead_t * target)
{
	model_script_t * script = (model_script_t *)self;
	bool isself = (target == NULL)? true : self->id == target->id;

	model_array_handlefree(script->links, MODEL_MAX_LINKS);
	model_array_handlefree(script->linkables, MODEL_MAX_LINKABLES);
	model_array_handlefree(script->modules, MODEL_MAX_MODULES);

	return isself;
}

static bool model_module_destroy(model_t * model, modelhead_t * self, modelhead_t * target)
{
	model_module_t * module = (model_module_t *)self;
	bool isself = (target == NULL)? true : self->id == target->id;

	model_array_handlefree(module->configparams, MODEL_MAX_CONFIGPARAMS);

	return isself;
}

static bool model_configparam_destroy(model_t * model, modelhead_t * self, modelhead_t * target)
{
	return (target == NULL)? true : self->id == target->id;
}

static bool model_linkable_destroy(model_t * model, modelhead_t * self, modelhead_t * target)
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

static bool model_link_destroy(model_t * model, modelhead_t * self, modelhead_t * target)
{
	model_link_t * link = (model_link_t *)self;
	bool isself = (target == NULL)? true : self->id == target->id;

	if (!isself)
	{
		// If any of the linkables we are linking is being deleted, delete ourselves too
		isself |= link->out->head.id == target->id;
		isself |= link->in->head.id == target->id;
	}

	return isself;
}



static inline void traverse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, void ** items, size_t maxsize)
{
	modelhead_t * item = NULL;
	model_foreach(item, items, maxsize)
	{
		item->analyse(model, traversal, cbs, item);
	}
}

#define model_handlecallback(callback, model, object) \
	if (callback != NULL) object->head.userdata = callback(object->head.userdata, model, object)

static void model_model_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, void * object)
{
	switch (traversal)
	{
		case traversal_scripts_modules_configs_linkables_links:
		{
			traverse(model, traversal, cbs, (void **)model->scripts, MODEL_MAX_SCRIPTS);
			break;
		}

		default: break;
	}
}

static void model_script_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, void * object)
{
	model_script_t * script = object;

	switch (traversal)
	{
		case traversal_scripts_modules_configs_linkables_links:
		{
			model_handlecallback(cbs->scripts, model, script);
			traverse(model, traversal, cbs, (void **)script->modules, MODEL_MAX_MODULES);
			traverse(model, traversal, cbs, (void **)script->linkables, MODEL_MAX_LINKABLES);
			traverse(model, traversal, cbs, (void **)script->links, MODEL_MAX_LINKS);
			break;
		}

		default: break;
	}
}

static void model_module_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, void * object)
{
	model_module_t * module = object;

	switch (traversal)
	{
		case traversal_scripts_modules_configs_linkables_links:
		{
			model_handlecallback(cbs->modules, model, module);
			traverse(model, traversal, cbs, (void **)module->configparams, MODEL_MAX_CONFIGPARAMS);
			break;
		}

		default: break;
	}
}

static void model_configparam_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, void * object)
{
	model_configparam_t * configparam = object;

	switch (traversal)
	{
		case traversal_scripts_modules_configs_linkables_links:
		{
			model_handlecallback(cbs->configs, model, configparam);
			break;
		}

		default: break;
	}
}

static void model_linkable_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, void * object)
{
	model_linkable_t * linkable = object;

	void linkable_handlecallbacks()
	{
		model_handlecallback(cbs->linkables, model, linkable);
		switch (linkable->head.type)
		{
			case model_blockinst:	model_handlecallback(cbs->blockinsts, model, linkable);		break;
			case model_syscall:		model_handlecallback(cbs->syscalls, model, linkable);		break;
			case model_rategroup:	model_handlecallback(cbs->rategroups, model, linkable);		break;
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

static void model_link_analyse(const model_t * model, modeltraversal_t traversal, const model_analysis_t * cbs, void * object)
{
	model_link_t * link = object;

	switch (traversal)
	{
		case traversal_scripts_modules_configs_linkables_links:
		{
			model_handlecallback(cbs->links, model, link);
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
	head->analyse = model_model_analyse;

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

void model_destroy(model_t * model)
{
	model_free(model, &model->head, &model->head);
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

const char * model_getpastconstraint(const char * desc)
{
	// TODO - FINISH ME!
	return NULL;
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

model_module_t * model_module_new(model_t * model, model_script_t * script, meta_t * meta, exception_t ** err)
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
			model_foreach(item, script->modules, MODEL_MAX_MODULES)
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
			model_foreach(item, model->backings, MODEL_MAX_BACKINGS)
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

		module = *mem = model_malloc(model, model_module, model_module_destroy, model_module_analyse, sizeof(model_module_t));
		if (module == NULL)
		{
			exception_set(err, ENOMEM, "Out of heap memory!");
			return NULL;
		}

		module->backing = backing;

		// Create static block instance
		module->staticinst = model_blockinst_new(model, module, script, "static", NULL, 0, err);
		if (module->staticinst == NULL || exception_check(err))
		{
			// An error happened!
			return NULL;
		}
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

model_configparam_t * model_configparam_new(model_t * model, model_module_t * module, const char * configname, const char * value, exception_t ** err)
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

		if (module->backing == NULL)
		{
			exception_set(err, EINVAL, "Module is unbound to meta object!");
			return NULL;
		}
	}

	const char * path = NULL;
	meta_getinfo(module->backing, &path, NULL, NULL, NULL, NULL);

	char sig;
	const char * desc;
	if (!meta_getconfigparam(module->backing, configname, &sig, &desc))
	{
		exception_set(err, EINVAL, "Module %s does not have config param %s", path, configname);
		return NULL;
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
		return NULL;
	}

	strcpy(configparam->name, configname);
	configparam->sig = sig;
	configparam->constraints = constraint_parse(desc, NULL);
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
	meta_getinfo(module->backing, &path, NULL, NULL, NULL, NULL);

	if (strcmp(blockname, "static") != 0)
	{
		const char * constructor_sig = NULL;
		if (!meta_getblock(module->backing, blockname, &constructor_sig, NULL, NULL))
		{
			exception_set(err, EINVAL, "Module %s does not have block %s", path, blockname);
			return NULL;
		}

		if (strlen(constructor_sig) != args_length)
		{
			exception_set(err, EINVAL, "Block %s constructor argument mismatch. Expected %zu argument(s) for signature %s, got %zu", blockname, strlen(constructor_sig), constructor_sig, args_length);
			return NULL;
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

	model_linkable_t * linkable = *mem = model_malloc(model, model_blockinst, model_linkable_destroy, model_linkable_analyse, sizeof(model_linkable_t) + sizeof(model_blockinst_t) + args_size);
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
		rategroup->blocks[i] = elems[i];
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

		if (desc != NULL && strlen(desc) >= MODEL_SIZE_DESCRIPTION)
		{
			exception_set(err, ENOMEM, "Syscall description is too long! (MODEL_SIZE_SHORTDESCRIPTION = %d)", MODEL_SIZE_DESCRIPTION);
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

model_link_t * model_link_new(model_t * model, model_script_t * script, model_linkable_t * out, const char * outname, model_linkable_t * in, const char * inname, exception_t ** err)
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

	// Verify that out_name and in_name are valid!
	switch (out->head.type)
	{
		case model_blockinst:
		{
			model_blockinst_t * blockinst = out->backing.blockinst;
			meta_t * backing = blockinst->module->backing;

			size_t ios_length = 0;
			if (!meta_getblock(backing, blockinst->name, NULL, &ios_length, NULL))
			{
				exception_set(err, EINVAL, "Could not get block instance IO length");
				return NULL;
			}

			const char * names[ios_length];
			metaiotype_t types[ios_length];
			if (!meta_getblockios(backing, blockinst->name, names, types, NULL, NULL, ios_length))
			{
				exception_set(err, EINVAL, "Could not get block instance IOs");
				return NULL;
			}

			bool found = false;
			for (size_t i = 0; i < ios_length; i++)
			{
				if (types[i] == meta_output && strcmp(names[i], outname) == 0)
				{
					found = true;
					break;
				}
			}

			if (!found)
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
			regex_t arg_pattern = {0};
			if (regcomp(&arg_pattern, "^a[0-9]+$", REG_EXTENDED | REG_NOSUB) != 0)
			{
				exception_set(err, EFAULT, "Bad model_syscall output regex!");
				return NULL;
			}

			if (regexec(&arg_pattern, outname, 0, NULL, 0) != 0)
			{
				exception_set(err, EINVAL, "Syscall has no output named '%s' (Only options: 'a1' ... 'a#')", outname);
				regfree(&arg_pattern);
				return NULL;
			}

			regfree(&arg_pattern);
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

			size_t ios_length = 0;
			if (!meta_getblock(backing, blockinst->name, NULL, &ios_length, NULL))
			{
				exception_set(err, EINVAL, "Could not get block instance IO length");
				return NULL;
			}

			const char * names[ios_length];
			metaiotype_t types[ios_length];
			if (!meta_getblockios(backing, blockinst->name, names, types, NULL, NULL, ios_length))
			{
				exception_set(err, EINVAL, "Could not get block instance IOs");
				return NULL;
			}

			bool found = false;
			for (size_t i = 0; i < ios_length; i++)
			{
				if (types[i] == meta_input && strcmp(names[i], inname) == 0)
				{
					found = true;
					break;
				}
			}

			if (!found)
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

	model_link_t * link = *mem = model_malloc(model, model_link, model_link_destroy, model_link_analyse, sizeof(model_link_t));
	if (link == NULL)
	{
		exception_set(err, ENOMEM, "Out of heap memory!");
		return NULL;
	}

	strcpy(link->out_name, outname);
	link->out = out;
	strcpy(link->in_name, inname);
	link->in = in;

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

	model->head.analyse(model, traversal, funcs, model);
}


bool model_getmeta(const model_module_t * module, const meta_t ** meta)
{
	// Sanity check
	{
		if (module == NULL)
		{
			return false;
		}
	}

	if (meta != NULL)
	{
		*meta = module->backing;
	}

	return true;
}

bool model_findmeta(const model_t * model, const char * path, const meta_t ** meta)
{
	// Sanity check
	{
		if (model == NULL || path == NULL)
		{
			return false;
		}
	}

	if (meta != NULL)
	{
		meta_t * testmeta = NULL;
		model_foreach(testmeta, model->backings, MODEL_MAX_BACKINGS)
		{
			const char * testpath = NULL;
			meta_getinfo(testmeta, &testpath, NULL, NULL, NULL, NULL);

			if (strcmp(path, testpath) == 0)
			{
				*meta = testmeta;
				return true;
			}
		}
	}

	return false;
}
