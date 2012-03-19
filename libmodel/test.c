#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#include <maxmeta.h>


meta_t * m = NULL;

/*
void __meta_init_(const meta_begin_t * begin)
{
	memcpy(m->buffer, begin, m->section_size);
}

__meta_begin_callback __meta_init = __meta_init_;
*/

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
	m = meta_parseelf("libtest2.so", &e);
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

	/*
	// TODO - find out if we can do RTLD_LOCAL
	void * d = dlopen("libtest2.so", RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
	if (d == NULL)
	{
		printf("* Fail dl: %s\n", dlerror());
	}
	*/

	if (!meta_loadmodule(m, &e))
	{
		printf("* Fail: Code %d %s\n", e->code, e->message);
		return -1;
	}

	print_meta(m);
	meta_free(m);

	return 0;
}

