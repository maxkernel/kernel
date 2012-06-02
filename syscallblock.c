#include <stdarg.h>

#include <kernel.h>
#include <kernel-priv.h>
#include <buffer.h>
#include <serialize.h>


static void syscallblock_dosyscall(void * ret, const void * args[], void * userdata)
{
	syscallblock_t * sb = userdata;

	mutex_lock(&sb->lock);
	{
		link_doinputs(&sb->ports, &sb->links);
		{

		}
		link_dooutputs(&sb->ports, &sb->links);
	}
	mutex_unlock(&sb->lock);

	/*
	io_beforeblock(sb->blockinst);
	{
		const char * param;
		size_t index = 0;

		foreach_methodparam(method_params(sb->syscall->sig), param)
		{
			if (*param != T_VOID)
			{
				string_t pname = string_new("p%zu", index);
				io_dooutput(sb->blockinst, pname.string, args[index++]);
			}
		}

		if (method_returntype(sb->syscall->sig) != T_VOID)
		{
			const void * r = io_doinput(sb->blockinst, "r");
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
	io_afterblock(sb->blockinst);
	*/
}

char * syscallblock_info(void * object)
{
	char * str = "[PLACEHOLDER SYSCALLBLOCK INFO]";
	return strdup(str);
}

void syscallblock_free(void * object)
{
	syscallblock_t * sb = object;
	closure_free(sb->closure);
}

/*
blockinst_t * syscallblock_getblockinst(syscallblock_t * sb)
{
	return sb->blockinst;
}
*/

syscallblock_t * syscallblock_new(const model_linkable_t * syscall, const char * name, const char * sig, const char * desc)
{
	// Sanity check
	{
		if (name == NULL || sig == NULL)
		{
			LOGK(LOG_ERR, "Could not create syscall block with name '%s' or invalid signature", name);
			return NULL;
		}

		const char * param = NULL;
		foreach_methodparam(method_params(sig), param)
		{
			switch (*param)
			{
				case T_BOOLEAN:
				case T_INTEGER:
				case T_DOUBLE:
				case T_CHAR:
				case T_STRING:
					continue;

				default:
					LOGK(LOG_ERR, "Could not create syscall block %s: invalid type '%s'", name, kernel_datatype(*param));
					return NULL;
			}
		}
	}

	//size_t argcount = method_numparams(method_params(sig));
	//size_t sb_size = sizeof(syscallblock_t) + (sizeof(iobacking_t *) * argcount);
	//syscallblock_t * sb = kobj_new("SyscallBlock", name, syscallblock_info, syscallblock_free, sb_size);
	syscallblock_t * sb = kobj_new("SyscallBlockinst", name, syscallblock_info, syscallblock_free, sizeof(syscallblock_t));
	mutex_init(&sb->lock, M_NORMAL);
	linklist_init(&sb->links);
	portlist_init(&sb->ports);

	syscall_f func = NULL;

	// Build the closure
	{
		exception_t * e = NULL;
		sb->closure = closure_build(&func, syscallblock_dosyscall, sig, sb, &e);
		if (sb->closure == NULL || exception_check(&e))
		{
			LOGK(LOG_ERR, "Could not create syscall block closure %s: %s", name, (e == NULL)? "Unknown error" : e->message);
			exception_free(e);
			return NULL;
		}
	}

	// Build syscall
	{
		exception_t * e = NULL;
		sb->syscall = syscall_new(name, sig, func, desc, &e);
		if (sb->syscall == NULL || exception_check(&e))
		{
			LOGK(LOG_ERR, "Could not create syscall %s: %s", name, (e == NULL)? "Unknown error" : e->message);
			exception_free(e);
			return NULL;
		}
	}

	// Build the biobacking return
	{
		exception_t * e = NULL;

		iobacking_t * backing = iobacking_new(method_returntype(sig), &e);
		if (backing == NULL || exception_check(&e))
		{
			LOGK(LOG_ERR, "Could not create syscall return backing for %s: %s", name, (e == NULL)? "Unknown error" : e->message);
			exception_free(e);
			return NULL;
		}

		port_t * port = port_new(meta_input, "r", backing, &e);
		if (port == NULL || exception_check(&e))
		{
			LOGK(LOG_ERR, "Could not create syscall return port for %s: %s", name, (e == NULL)? "Unknown error" : e->message);
			exception_free(e);
			return NULL;
		}

		port_add(&sb->ports, port);
	}

	// Build the biobacking arguments
	{
		exception_t * e = NULL;

		const char * param = NULL;
		size_t index = 0;
		foreach_methodparam(method_params(sig), param)
		{
			iobacking_t * backing = iobacking_new(*param, &e);
			if (backing == NULL || exception_check(&e))
			{
				LOGK(LOG_ERR, "Could not create syscall arg[%zu] backing for %s: %s", index, name, (e == NULL)? "Unknown error" : e->message);
				exception_free(e);
				return NULL;
			}

			string_t port_name = string_new("a%zu", index);
			port_t * port = port_new(meta_output, port_name.string, backing, &e);
			if (port == NULL || exception_check(&e))
			{
				LOGK(LOG_ERR, "Could not create syscall arg[%zu] port for %s: %s", index, name, (e == NULL)? "Unknown error" : e->message);
				exception_free(e);
				return NULL;
			}

			port_add(&sb->ports, port);

			index += 1;
		}
	}

	// Build block
	/* TODO - finish me!
	{
		string_t blkobjname = string_new("%s(%s) syscall block", sb->syscall->name, sb->syscall->sig);
		string_t blkname = string_new("syscall_%s", name);
		string_t blkdesc = string_new("Syscall block for %s(%s)", sb->syscall->name, sb->syscall->sig);

		block_t * blk = kobj_new("Block", string_copy(&blkobjname), io_blockinfo, io_blockfree, sizeof(block_t));
		blk->name = string_copy(&blkname);
		blk->desc = string_copy(&blkdesc);
		LIST_INIT(&blk->inputs);
		LIST_INIT(&blk->outputs);

		for(size_t index = 0; index < numparams; index++)
		{
			string_t pname = string_new("p%zu", index);

			bio_t * out = malloc0(sizeof(bio_t));
			out->block = blk;
			out->name = string_copy(&pname);
			out->desc = STRDUP(params[index]->input->desc);
			out->sig = params[index]->input->sig;

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

		sb->block = blk;
	}
	*/

	// Create block instance
	//sb->blockinst = io_newblockinst(sb->block, NULL);
	//sb->blockinst = blockinst_new(NULL, syscall, name);

	// Route the data
	/* TODO - finish me!
	{
		for (size_t index = 0; index < numparams; index++)
		{
			string_t pname = string_new("p%zu", index);
			boutput_inst_t * out = io_getboutput(sb->blockinst, pname.string);
			if (out != NULL)
			{
				io_route(out, params[index]);
			}
			else
			{
				LOGK(LOG_FATAL, "Could not route %s -> %s in syscall block! This error should be impossible to trigger!", pname.string, params[index]->input->name);
				// Will exit
			}
		}

		if (ret != NULL)
		{
			string_t rname = string_new("r");
			binput_inst_t * in = io_getbinput(sb->blockinst, rname.string);
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
	*/

	return sb;
}
