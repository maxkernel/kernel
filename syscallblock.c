#include <stdarg.h>

#include "kernel.h"
#include "kernel-priv.h"
#include "buffer.h"
#include "serialize.h"


extern module_t * kernel_module;


static void syscallblock_dosyscall(void * object, ...)
{
	syscallblock_t * sb = object;

	va_list args;
	va_start(args, object);

	String pname;
	const char * sig = method_params(sb->syscall->sig);
	size_t sigi = 0;

	while (*sig != '\0')
	{
		string_clear(&pname);
		string_append(&pname, "p%zu", sigi+1);

		switch (*sig)
		{
			#define __sb_dosyscall_elem(t1, t2) \
				case t1: {\
					t2 v = va_arg(args, t2); \
					io_dooutput(sb->block_inst, pname.string, &v, false); \
					break; }

			__sb_dosyscall_elem(T_BOOLEAN, int)
			__sb_dosyscall_elem(T_INTEGER, int)
			__sb_dosyscall_elem(T_DOUBLE, double)
			__sb_dosyscall_elem(T_CHAR, int)
			__sb_dosyscall_elem(T_STRING, char *)
			__sb_dosyscall_elem(T_BUFFER, buffer_t)
			__sb_dosyscall_elem(T_ARRAY_BOOLEAN, buffer_t)
			__sb_dosyscall_elem(T_ARRAY_INTEGER, buffer_t)
			__sb_dosyscall_elem(T_ARRAY_DOUBLE, buffer_t)
		}

		sig++;
		sigi++;
	}

	va_end(args);

	io_afterblock(sb->block_inst);
}

char * syscallblock_info(void * object)
{
	char * str = "[PLACEHOLDER SYSCALLBLOCK INFO]";
	return strdup(str);
}

void syscallblock_free(void * object)
{
	//TODO - free me
}

block_inst_t * syscallblock_getblockinst(syscallblock_t * sb)
{
	return sb->block_inst;
}

syscallblock_t * syscallblock_new(const char * name, binput_inst_t ** params, const char * desc)
{
	syscallblock_t * sb = kobj_new("SyscallBlock", strdup(name), syscallblock_info, syscallblock_free, sizeof(syscallblock_t));

	//build syscall
	{
		syscall_t * syscall = malloc0(sizeof(syscall_t));
		syscall->dynamic_data = sb;
		syscall->func = (void *)syscallblock_dosyscall;
		syscall->name = strdup(name);
		syscall->desc = STRDUP(desc);

		String sig = string_new("v:");
		int i=0;
		for (; params[i] != NULL; i++)
		{
			string_append(&sig, "%c", params[i]->input->sig);
		}
		syscall->sig = string_copy(&sig);

		syscall_reg(syscall);
		sb->syscall = syscall;
	}


	//build block
	{
		String blkobjname = string_new("%s(%s) syscall block", sb->syscall->name, sb->syscall->sig);
		String blkname = string_new("syscall_%s", name);
		String blkdesc = string_new("Syscall block for %s(%s)", sb->syscall->name, sb->syscall->sig);

		block_t * blk = kobj_new("Block", string_copy(&blkobjname), io_blockinfo, io_blockfree, sizeof(block_t));

		blk->name = string_copy(&blkname);
		blk->desc = string_copy(&blkdesc);
		blk->module = kernel_module;
		LIST_INIT(&blk->outputs);

		String pname;
		size_t i=0;
		for(; params[i] != NULL; i++)
		{
			string_clear(&pname);
			string_append(&pname, "p%zu", i+1);

			bio_t * out = malloc0(sizeof(bio_t));
			out->block = blk;
			out->name = string_copy(&pname);
			out->desc = STRDUP(params[i]->input->desc);
			out->sig = params[i]->input->sig;

			list_add(&blk->outputs, &out->block_list);
		}

		list_add(&kernel_module->blocks, &blk->module_list);
		sb->block = blk;
	}

	//create block instance
	sb->block_inst = io_newblock(sb->block, NULL);

	//route the data
	{
		String pname;
		size_t i=0;
		for (; params[i] != NULL; i++)
		{
			string_clear(&pname);
			string_append(&pname, "p%zu", i+1);

			list_t * pos;
			boutput_inst_t * out = NULL;
			list_foreach(pos, &sb->block_inst->outputs_inst)
			{
				boutput_inst_t * test = list_entry(pos, boutput_inst_t, block_inst_list);
				if (strcmp(pname.string, test->output->name) == 0)
				{
					out = test;
					break;
				}
			}

			if (out != NULL)
			{
				io_route(out, params[i]);
			}
			else
			{
				LOGK(LOG_FATAL, "Could not route %s -> %s in syscall block! This error should be impossible to trigger!", pname.string, params[i]->input->name);
				// Will exit
			}
		}
	}

	return sb;
}

