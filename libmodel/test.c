#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#include <maxmeta.h>


meta_t * m = NULL;

void __meta_init_(const meta_begin_t * begin)
{
	memcpy(m->buffer, begin, m->section_size);
}

__meta_begin_callback __meta_init = __meta_init_;


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

	//meta_variable_t * config_param = NULL;


	/*

	printf("Config params: %d\n", m->configparams_length);
	for (size_t i = 0; i < m->configparams_length; i++)
	{
		printf("(%zu) Config param: %s (%c) %s -> %zx\n", i, m->configparams[i].config_name, m->configparams[i].config_signature, m->configparams[i].config_description, (size_t)m->configparams[i].variable);
	}

	printf("Cal modechange: %s -> %zx\n", m->cal_modechange_name, (size_t)m->cal_modechange);
	printf("Cal preview: %s -> %zx\n", m->cal_preview_name, (size_t)m->cal_preview);
	printf("Cal params: %d\n", m->calparams_length);
	for (size_t i = 0; i < m->configparams_length; i++)
	{
		printf("(%zu) Cal param: %s (%c) %s -> %zx\n", i, m->calparams[i].cal_name, m->calparams[i].cal_signature, m->calparams[i].cal_description, (size_t)m->calparams[i].variable);
	}

	printf("Blocks: %d\n", m->blocks_length);
	for (size_t i = 0; i < m->blocks_length; i++)
	{
		printf("(%zu) Block: %s %s\n", i, m->blocks[i].block_name, m->blocks[i].block_description);
		printf("(%zu) Constructor: %s (%s) %s -> %zx\n", i, m->blocks[i].constructor_name, m->blocks[i].constructor_signature, m->blocks[i].constructor_description, (size_t)m->blocks[i].constructor);
		printf("(%zu) Block funcs: %s -> %zx, %s -> %zx\n", i, m->blocks[i].update_name, (size_t)m->blocks[i].update, m->blocks[i].destroy_name, (size_t)m->blocks[i].destroy);

		printf("Ios: %d\n", m->blocks[i].ios_length);
		for (size_t j = 0; j < m->blocks[i].ios_length; j++)
		{
			printf("(%zu) %c %s (%c) %s\n", j, (char)m->blocks[i].ios[j].io_type, m->blocks[i].ios[j].io_name, m->blocks[i].ios[j].io_signature, m->blocks[i].ios[j].io_description);
		}
	}
	*/
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

	// TODO - find out if we can do RTLD_LOCAL
	void * d = dlopen("libtest2.so", RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND);
	if (d == NULL)
	{
		printf("* Fail dl: %s\n", dlerror());
	}

	print_meta(m);
	meta_free(m);

	return 0;
}

