#include <stdio.h>
#include <string.h>

#include <maxmeta.h>


bool mod_init()
{
    printf("Init!\n");
    return true;
}

void mod_destroy()
{
	printf("Destroy!\n");
}

void mod_preact()
{
	printf("Preact!\n");
}

void mod_postact()
{
	printf("Postact!\n");
}

int new_syscall(const char * str, char c)
{
	return 0;
}

int cfg_param = 6;
int cal_param = 7;

void cal_modechange(int newmode)
{

}

bool cal_preview(const char * name, const char sig, void * oldval, void * newval)
{
	return false;
}

void * tblock_new(bool v1, int v2, double v3)
{
	return NULL;
}

void tblock_update(void * object)
{

}

void tblock_destroy(void * object)
{

}

module_name("Test Module");
module_version(3.5);
module_author("Andrew Klofas <andrew@maxkernel.com>");
module_description("This is a description");
module_dependency("dependency1");

module_oninitialize(mod_init);
module_ondestroy(mod_destroy);
module_onpreactivate(mod_preact);
module_onpostactivate(mod_postact);

define_syscall(new_syscall, "i:sc", "The syscall description");

config_param(cfg_param, 'i', "Config param desc");

cal_param(cal_param, 'i', "Cal param desc");
cal_onmodechange(cal_modechange);
cal_onpreview(cal_preview);

define_block(test_block, "Test block desc", tblock_new, "v:bid", "Constructor desc");
block_onupdate(test_block, tblock_update);
block_ondestroy(test_block, tblock_destroy);
block_input(test_block, tinput1, 'i', "Input 1 desc");
block_input(test_block, tinput, 'b', "Input 2 desc");
block_output(test_block, toutput1, 's', "Output 1 desc");


int main()
{
    printf("Begin\n");
    printf("Size of meta_t: %zu\n", sizeof(meta_t));
    
    meta_t m;
    memset(&m, 0, sizeof(m));
    
    exception_t * e = NULL;
    if (!meta_parseelf(&m, "test", &e))
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
    
    printf("Name: %s\n", m.name);
    printf("Version: %f\n", m.version);
    printf("Author: %s\n", m.author);
    printf("Description: %s\n", m.description);

    printf("Dependencies: %d\n", m.dependencies_length);
    for (size_t i = 0; i < m.dependencies_length; i++)
    {
    	printf("(%zu) Dependency: %s\n", i, m.dependencies[i]);
    }

    printf("Init: %s -> %zu\n", m.init_name, (size_t)m.init);
    printf("Destroy: %s -> %zu\n", m.destroy_name, (size_t)m.destroy);
    printf("Preact: %s -> %zu\n", m.preact_name, (size_t)m.preact);
    printf("Postact: %s -> %zu\n", m.postact_name, (size_t)m.postact);

    printf("Syscalls: %d\n", m.syscalls_length);
    for (size_t i = 0; i < m.syscalls_length; i++)
    {
    	printf("(%zu) Syscall: %s (%s) %s -> %zu\n", i, m.syscalls[i].syscall_name, m.syscalls[i].syscall_signature, m.syscalls[i].syscall_description, (size_t)m.syscalls[i].function);
    }

    printf("Config params: %d\n", m.configparams_length);
    for (size_t i = 0; i < m.configparams_length; i++)
    {
    	printf("(%zu) Config param: %s (%c) %s -> %zu\n", i, m.configparams[i].config_name, m.configparams[i].config_signature, m.configparams[i].config_description, (size_t)m.configparams[i].variable);
    }

    printf("Cal modechange: %s -> %zu\n", m.cal_modechange_name, (size_t)m.cal_modechange);
    printf("Cal preview: %s -> %zu\n", m.cal_preview_name, (size_t)m.cal_preview);
    printf("Cal params: %d\n", m.calparams_length);
	for (size_t i = 0; i < m.configparams_length; i++)
	{
		printf("(%zu) Cal param: %s (%c) %s -> %zu\n", i, m.calparams[i].cal_name, m.calparams[i].cal_signature, m.calparams[i].cal_description, (size_t)m.calparams[i].variable);
	}

	printf("Blocks: %d\n", m.blocks_length);
	for (size_t i = 0; i < m.blocks_length; i++)
	{
		printf("(%zu) Block: %s %s\n", i, m.blocks[i].block_name, m.blocks[i].block_description);
		printf("(%zu) Constructor: %s (%s) %s -> %zu\n", i, m.blocks[i].constructor_name, m.blocks[i].constructor_signature, m.blocks[i].constructor_description, (size_t)m.blocks[i].constructor);
		printf("(%zu) Block funcs: %s -> %zu, %s -> %zu\n", i, m.blocks[i].update_name, (size_t)m.blocks[i].update, m.blocks[i].destroy_name, (size_t)m.blocks[i].destroy);

		printf("Ios: %d\n", m.blocks[i].ios_length);
		for (size_t j = 0; j < m.blocks[i].ios_length; j++)
		{
			printf("(%zu) %c %s (%c) %s\n", j, (char)m.blocks[i].ios[j].io_type, m.blocks[i].ios[j].io_name, m.blocks[i].ios[j].io_signature, m.blocks[i].ios[j].io_description);
		}
	}

    return 0;
}

