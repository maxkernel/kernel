#include <stdarg.h>

#include <kernel.h>
#include <kernel-priv.h>
#include <buffer.h>
#include <serialize.h>


extern module_t * kernel_module;

static void syscallblock_dosyscall(void * ret, const void * args[], void * userdata)
{
	syscallblock_t * sb = userdata;

	io_beforeblock(sb->block_inst);
	{
		const char * param;
		size_t index = 1;

		foreach_methodparam(method_params(sb->syscall->sig), param)
		{
			if (*param != T_VOID)
			{
				string_t pname = string_new("p%zu", index);
				io_dooutput(sb->block_inst, pname.string, args[index++]);
			}
		}

		if (method_returntype(sb->syscall->sig) != T_VOID)
		{
			const void * r = io_doinput(sb->block_inst, "r");
			if (r != NULL)
			{
				switch (method_returntype(sb->syscall->sig))
				{
					case T_BOOLEAN:		memcpy(ret, r, sizeof(bool));	break;
					case T_INTEGER:		memcpy(ret, r, sizeof(int));	break;
					case T_DOUBLE:		memcpy(ret, r, sizeof(double));	break;
					case T_CHAR:		memcpy(ret, r, sizeof(char));	break;
					case T_STRING:		memcpy(ret, r, sizeof(char *));	break;
				}
			}
			else
			{
				switch (method_returntype(sb->syscall->sig))
				{
					case T_BOOLEAN:		*(bool *)ret = false;		break;
					case T_INTEGER:		*(int *)ret = 0;			break;
					case T_DOUBLE:		*(double *)ret = 0.0;		break;
					case T_CHAR:		*(char *)ret = '\0';		break;
					case T_STRING:		*(char **)ret = "";			break;
				}
			}
		}
	}
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

syscallblock_t * syscallblock_new(const char * name, boutput_inst_t * ret, binput_inst_t ** params, size_t numparams, const char * desc)
{
	// Sanity check
	{
		if (name == NULL || params == NULL)
		{
			LOGK(LOG_ERR, "Could not create syscall block with name '%s' or invalid parameters", name);
			return NULL;
		}

		for (size_t i=0; i<numparams; i++)
		{
			switch (params[i]->input->sig)
			{
				case T_BOOLEAN:
				case T_INTEGER:
				case T_DOUBLE:
				case T_CHAR:
				case T_STRING:
					continue;

				default:
					LOGK(LOG_ERR, "Could not create syscall block %s: invalid type for parameter %zu: %s", name, i, kernel_datatype(params[i]->input->sig));
					return NULL;
			}
		}
	}

	syscallblock_t * sb = kobj_new("SyscallBlock", strdup(name), syscallblock_info, syscallblock_free, sizeof(syscallblock_t));

	// Build syscall
	{
		sb->syscall = malloc0(sizeof(syscall_t));
		//syscall->dynamic_data = sb;
		//syscall->func = syscallblock_getfunction(ret == NULL? T_VOID : ret->output->sig);
		sb->syscall->name = strdup(name);
		sb->syscall->desc = STRDUP(desc);

		string_t sig = string_new("%c:", ret == NULL? 'v' : ret->output->sig);
		int i=0;
		for (; i<numparams; i++)
		{
			string_append(&sig, "%c", params[i]->input->sig);
		}
		sb->syscall->sig = string_copy(&sig);
	}

	// Build the closure
	{
		exception_t * e = NULL;
		sb->closure = closure_build(&sb->syscall->func, syscallblock_dosyscall, sb->syscall->sig, sb, &e);

		if (sb->closure == NULL)
		{
			LOGK(LOG_ERR, "Could not create syscall block closure %s: %s", name, e->message);
			return NULL;
		}
	}

	// Register the syscall
	{
		syscall_reg(sb->syscall);
	}

	// Build block
	{
		string_t blkobjname = string_new("%s(%s) syscall block", sb->syscall->name, sb->syscall->sig);
		string_t blkname = string_new("syscall_%s", name);
		string_t blkdesc = string_new("Syscall block for %s(%s)", sb->syscall->name, sb->syscall->sig);

		block_t * blk = kobj_new("Block", string_copy(&blkobjname), io_blockinfo, io_blockfree, sizeof(block_t));

		blk->name = string_copy(&blkname);
		blk->desc = string_copy(&blkdesc);
		blk->module = kernel_module;
		LIST_INIT(&blk->inputs);
		LIST_INIT(&blk->outputs);

		for(size_t i=0; i<numparams; i++)
		{
			string_t pname = string_new("p%zu", i+1);

			bio_t * out = malloc0(sizeof(bio_t));
			out->block = blk;
			out->name = string_copy(&pname);
			out->desc = STRDUP(params[i]->input->desc);
			out->sig = params[i]->input->sig;

			list_add(&blk->outputs, &out->block_list);
		}

		if (ret != NULL)
		{
			bio_t * in = malloc0(sizeof(bio_t));
			in->block = blk;
			in->name = strdup("r");
			in->desc = STRDUP(ret->output->desc);
			in->sig = ret->output->sig;

			list_add(&blk->inputs, &in->block_list);
		}

		list_add(&kernel_module->blocks, &blk->module_list);
		sb->block = blk;
	}

	// Create block instance
	sb->block_inst = io_newblock(sb->block, NULL);

	// Route the data
	{
		for (size_t i = 0; i < numparams; i++)
		{
			string_t pname = string_new("p%zu", i+1);
			boutput_inst_t * out = io_getboutput(sb->block_inst, pname.string);
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

		if (ret != NULL)
		{
			string_t rname = string_new("r");
			binput_inst_t * in = io_getbinput(sb->block_inst, rname.string);
			if (in != NULL)
			{
				io_route(ret, in);
			}
			else
			{
				LOGK(LOG_FATAL, "Could not route %s -> %s in syscall block! This error should be impossible to trigger!", ret->output->name, rname.string);
				// Will exit
			}
		}
	}
	return sb;
}

