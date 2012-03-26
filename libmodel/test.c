#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#include <maxmodel/meta.h>
#include <maxmodel/interpret.h>


void print_meta(meta_t * m)
{
	printf("-------------------------------\n");
	printf("Path: %s\n", m->path);
	printf("Name: %s\n", m->name->name);

	string_t v = version_tostring(m->version->version);
	printf("Version: %s\n", v.string);
	printf("Author: %s\n", m->author->name);
	printf("Description: %s\n", m->description->description);

	meta_dependency_t * dependency = NULL;
	meta_foreach(dependency, m->dependencies, META_MAX_DEPENDENCIES)
	{
		printf("-- Dependency: %s\n", dependency->dependency);
	}

	printf("Init: %s -> %zx\n", m->init->callback.function_name, (size_t)m->init->function);
	printf("Destroy: %s -> %zx\n", m->destroy->callback.function_name, (size_t)m->destroy->function);
	printf("Preact: %s -> %zx\n", m->preact->callback.function_name, (size_t)m->preact->function);
	printf("Postact: %s -> %zx\n", m->postact->callback.function_name, (size_t)m->postact->function);

	meta_callback_t * syscall = NULL;
	meta_foreach(syscall, m->syscalls, META_MAX_SYSCALLS)
	{
		printf("-- Syscall: %s (%s) %s -> %zx\n", syscall->callback.function_name, syscall->callback.function_signature, syscall->callback.function_description, (size_t)syscall->function);
	}

	meta_variable_t * config_param = NULL;
	meta_foreach(config_param, m->config_params, META_MAX_CONFIGPARAMS)
	{
		printf("-- Config Param: %s (%c) %s -> %zx\n", config_param->variable_name, config_param->variable_signature, config_param->variable_description, (size_t)config_param->variable);
	}

	meta_variable_t * cal_param = NULL;
	printf("Cal modechange: %s -> %zx\n", m->cal_modechange->callback.function_name, (size_t)m->cal_modechange->function);
	printf("Cal preview: %s -> %zx\n", m->cal_preview->callback.function_name, (size_t)m->cal_preview->function);
	meta_foreach(cal_param, m->cal_params, META_MAX_CALPARAMS)
	{
		printf("-- Cal Param: %s (%c) %s -> %zx\n", cal_param->variable_name, cal_param->variable_signature, cal_param->variable_description, (size_t)cal_param->variable);
	}

	meta_block_t * block = NULL;
	meta_foreach(block, m->blocks, META_MAX_BLOCKS)
	{
		printf("-- Block: %s %s -- %s (%s) %s -> %zx\n", block->block_name, block->block_description, block->constructor_name, block->constructor_signature, block->constructor_description, (size_t)block->constructor);
	}

	meta_blockcallback_t * blockcb = NULL;
	meta_foreach(blockcb, m->block_callbacks, META_MAX_BLOCKCBS)
	{
		printf("-- Block Callback: %s -- %x %s -> %zx\n", blockcb->blockname, blockcb->callback.head.type, blockcb->callback.function_name, (size_t)blockcb->function);
	}

	meta_blockio_t * blockio = NULL;
	meta_foreach(blockio, m->block_ios, META_MAX_BLOCKIOS)
	{
		printf("-- Block IO: %s -- %x %s (%c) %s\n", blockio->blockname, blockio->head.type, blockio->io_name, blockio->io_signature, blockio->io_description);
	}
}

int main()
{
	printf("Begin\n");
	printf("Size of meta_t: %zu\n", sizeof(meta_t));

	exception_t * e = NULL;
	meta_t * m = meta_parseelf("libtest2.so", &e);
	if (m == NULL)
	{
		if (e != NULL)
		{
			printf("* Fail: Code %d %s\n", e->code, e->message);
		}
		else
		{
			printf("* Fail with exception NULL!!\n");
		}

		return -1;
	}

	printf("Size of section: %zu\n", m->section_size);
	print_meta(m);

	if (!meta_loadmodule(m, &e))
	{
		printf("* Fail: Code %d %s\n", e->code, e->message);
		return -1;
	}

	print_meta(m);
	meta_destroy(m);


	void cbs_log(level_t level, const char * message)
	{
		printf("L %X: %s\n", level, message);
	}

	meta_t * cbs_metalookup(const char * modulename, exception_t ** err)
	{
		return meta_parseelf(modulename, err);
	}

	interpret_callbacks cbs = {
		.log = cbs_log,
		.metalookup = cbs_metalookup,
	};

	model_t * model = model_new();
	if (!interpret_lua(model, "test.lua", &cbs, &e))
	{
		printf("* Fail: Code %d %s\n", e->code, e->message);
		return -1;
	}


	void * cbs_scripts(void * udata, const model_script_t * script)
	{
		printf("Script: %s\n", script->path);
		return NULL;
	}

	void * cbs_modules(void * udata, const model_module_t * module)
	{
		printf("Module: %s -> %s\n", module->backing->path, module->backing->name);
		return NULL;
	}

	void * cbs_configs(void * udata, const model_configparam_t * configparam)
	{
		printf("Config param: %s (%c) = %s\n", configparam->name, configparam->sig, configparam->value);
		return NULL;
	}

	void * cbs_linkables(void * udata, const model_linkable_t * linkable)
	{
		printf("Linkable\n");
		return NULL;
	}

	void * cbs_blockinsts(void * udata, const model_linkable_t * linkable)
	{
		const model_blockinst_t * blockinst = linkable->backing.blockinst;
		printf("Block inst: %s ->", blockinst->name);
		for (size_t i = 0; i < MODEL_MAX_ARGS; i++)
		{
			if (blockinst->args[i] == NULL)
			{
				break;
			}

			printf(" (%zu) %s,", i, blockinst->args[i]);
		}
		printf("\n");

		return NULL;
	}

	void * cbs_rategroups(void * udata, const model_linkable_t * linkable)
	{
		const model_rategroup_t * rategroup = linkable->backing.rategroup;
		printf("Rategroup: %s (%f)\n", rategroup->name, rategroup->hertz);
		return NULL;
	}

	void * cbs_links(void * udata, const model_link_t * link)
	{
		printf("Link: %s -> %s\n", link->out_name, link->in_name);
		return NULL;
	}

	model_analysis_t cbs2 = {
		.scripts = cbs_scripts,
		.modules = cbs_modules,
		.configs = cbs_configs,
		.blockinsts = cbs_blockinsts,
		.rategroups = cbs_rategroups,
		.linkables = cbs_linkables,
		.links = cbs_links,
	};

	model_analyse(model, traversal_scripts_modules_configs_linkables_links, &cbs2);
	model_destroy(model);

	return 0;
}

