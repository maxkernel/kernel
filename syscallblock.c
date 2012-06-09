#include <stdarg.h>

#include <maxmodel/model.h>

#include <buffer.h>
#include <serialize.h>
#include <kernel.h>
#include <kernel-priv.h>


static char * syscallblock_info(void * object)
{
	unused(object);

	char * str = "[PLACEHOLDER SYSCALLBLOCK INFO]";
	return strdup(str);
}

static void syscallblock_free(void * object)
{
	syscallblock_t * sb = object;
	closure_free(sb->closure);
}

static void syscallblock_dosyscall(void * ret, const void * args[], void * userdata)
{
	syscallblock_t * sb = userdata;

	mutex_lock(&sb->lock);
	{
		link_doinputs(&sb->ports, &sb->links);
		{
			size_t index = 0;

			const char * param = NULL;
			method_foreachparam(param, method_params(sb->syscall->sig))
			{
				if unlikely(*param == T_VOID)
				{
					continue;
				}

				string_t name = string_new("a%zu", index);

				port_t * port = port_lookup(&sb->ports, meta_output, name.string);
				if unlikely(port == NULL)
				{
					LOGK(LOG_WARN, "Could not find argument '%s' port in syscall block instance '%s'!", name.string, sb->syscall->name);
					continue;
				}

				iobacking_copy(port_iobacking(port), args[index]);

				index += 1;
			}

			if (method_returntype(sb->syscall->sig) != T_VOID)
			{
				port_t * port = port_lookup(&sb->ports, meta_input, "r");
				if likely(port != NULL)
				{
					iobacking_t * backing = port_iobacking(port);
					if (!iobacking_isnull(backing))
					{
						const void * r = iobacking_data(backing);
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
				else
				{
					LOGK(LOG_WARN, "Could not find return port in syscall block instance '%s'!", sb->syscall->name);
				}
			}
		}
		link_dooutputs(&sb->ports, &sb->links);
	}
	mutex_unlock(&sb->lock);
}

syscallblock_t * syscallblock_new(const model_linkable_t * linkable, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(linkable == NULL || model_type(model_object(linkable)) != model_syscall)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}

	const char * name = NULL, * sig = NULL, * desc = NULL;
	model_getsyscall(linkable, &name, &sig, &desc);

	LOGK(LOG_DEBUG, "Creating syscall %s(%s)", name, sig);

	syscallblock_t * sb = kobj_new("SyscallBlockinst", name, syscallblock_info, syscallblock_free, sizeof(syscallblock_t));
	mutex_init(&sb->lock, M_NORMAL);
	linklist_init(&sb->links);
	portlist_init(&sb->ports);

	syscall_f func = NULL;

	// Build the closure
	{
		sb->closure = closure_build(&func, syscallblock_dosyscall, sig, sb, err);
		if (sb->closure == NULL || exception_check(err))
		{
			return NULL;
		}
	}

	// Build syscall
	{
		sb->syscall = syscall_new(name, sig, func, desc, err);
		if (sb->syscall == NULL || exception_check(err))
		{
			return NULL;
		}
	}

	// Build the biobacking return
	{
		iobacking_t * backing = iobacking_new(method_returntype(sig), err);
		if (backing == NULL || exception_check(err))
		{
			return NULL;
		}

		port_t * port = port_new(meta_input, "r", backing, err);
		if (port == NULL || exception_check(err))
		{
			return NULL;
		}

		port_add(&sb->ports, port);
	}

	// Build the biobacking arguments
	{
		size_t index = 0;

		const char * param = NULL;
		method_foreachparam(param, method_params(sig))
		{
			iobacking_t * backing = iobacking_new(*param, err);
			if (backing == NULL || exception_check(err))
			{
				return NULL;
			}

			string_t port_name = string_new("a%zu", index);
			port_t * port = port_new(meta_output, port_name.string, backing, err);
			if (port == NULL || exception_check(err))
			{
				return NULL;
			}

			port_add(&sb->ports, port);

			index += 1;
		}
	}

	return sb;
}

