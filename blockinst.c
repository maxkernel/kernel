#include <stdio.h>
#include <string.h>

#include <aul/iterator.h>
#include <aul/parse.h>

#include <method.h>
#include <serialize.h>
#include <kernel.h>
#include <kernel-priv.h>


static ssize_t blockinst_desc(const kobject_t * object, char * buffer, size_t length)
{
	const blockinst_t * blkinst = (const blockinst_t *)object;

	string_t args = string_blank();
	for (size_t i = 0; i < blkinst->argslen; i++)
	{
		string_append(&args, "%s'%s'", (i > 0)? ", " : "", blkinst->args[i]);
	}
	return snprintf(buffer, length, "{ 'name': '%s', 'block_id': '%#x', 'args': [ %s ] }", blkinst->name, kobj_id(kobj_cast(blkinst->block)), args.string);
}

static void blockinst_destroy(kobject_t * object)
{
	blockinst_t * blkinst = (blockinst_t *)object;
	blockinst_act(blkinst, block_cbdestroy(blockinst_block(blkinst)));
	link_destroy(&blkinst->links);
	free(blkinst->name);
}

blockinst_t * blockinst_new(const model_linkable_t * linkable, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(linkable == NULL || model_type(model_object(linkable)) != model_blockinst)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}


	const char * block_name = NULL;
	const model_module_t * module = NULL;
	const char * new_sig = NULL;
	const char * const * new_args = NULL;
	size_t new_argslen = 0;
	model_getblockinst(linkable, &block_name, &module, &new_sig, &new_args, &new_argslen);
	if (module == NULL)
	{
		exception_set(err, EFAULT, "Could not get module backing for block '%s'", block_name);
		return NULL;
	}

	const char * module_path = NULL;
	const meta_t * meta = NULL;
	model_getmodule(module, &module_path, &meta);
	if (meta == NULL || module_path == NULL)
	{
		exception_set(err, EFAULT, "Could not get meta object for module backing block '%s'", block_name);
		return NULL;
	}

	const char * module_name = NULL;
	meta_getinfo(meta, NULL, &module_name, NULL, NULL, NULL);

	LOGK(LOG_DEBUG, "Creating block instance '%s' in module %s", block_name, module_path);

	if (method_numparams(new_sig) != new_argslen)
	{
		exception_set(err, EINVAL, "Bad constructor arguments given to block '%s' in module %s", block_name, module_path);
		return NULL;
	}

	module_t * m = model_userdata(model_object(module));
	if (m == NULL)
	{
		exception_set(err, EFAULT, "Module object not set in module!");
		return NULL;
	}

	block_t * b = module_lookupblock(m, block_name);

	string_t name = string_new("%s.%s", module_name, block_name);
	blockinst_t * bi = kobj_new("Block Instance", name.string, blockinst_desc, blockinst_destroy, sizeof(blockinst_t));
	linklist_init(&bi->links);
	bi->block = b;
	bi->name = strdup(name.string);
	bi->args = new_args;			// TODO - do some sort of strcpy here!
	bi->argslen = new_argslen;

	block_add(b, bi);
	return bi;
}

bool blockinst_create(blockinst_t * blockinst, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(blockinst == NULL)
		{
			return false;
		}
	}


	ssize_t parse_arg(void * array, size_t arraylen, exception_t ** err, const char * sig, int index)
	{
		switch (sig[index])
		{
			case T_VOID:
			{
				return 0;
			}

			case T_BOOLEAN:
			{
				bool v = parse_bool(blockinst->args[index], err);
				memcpy(array, &v, min(arraylen, sizeof(bool)));
				return sizeof(bool);
			}

			case T_INTEGER:
			{
				int v = parse_int(blockinst->args[index], err);
				memcpy(array, &v, min(arraylen, sizeof(int)));
				return sizeof(int);
			}

			case T_DOUBLE:
			{
				double v = parse_double(blockinst->args[index], err);
				memcpy(array, &v, min(arraylen, sizeof(double)));
				return sizeof(double);
			}

			case T_CHAR:
			{
				memcpy(array, &blockinst->args[index][0], min(arraylen, sizeof(char)));
				return sizeof(char);
			}

			case T_STRING:
			{
				size_t len = strlen(blockinst->args[index]) + 1;
				memcpy(array, blockinst->args[index], min(arraylen, len));
				return len;
			}

			default:
			{
				exception_set(err, EINVAL, "Could not parse sig '%c'", sig[index]);
				return -1;
			}
		}
	}

	char buffer[BLOCKINST_BUFFERMAX];
	void ** header = (void **)buffer;
	ssize_t wrote = serialize_2array_fromcb_wheader(header, BLOCKINST_BUFFERMAX, err, block_newsignature(blockinst_block(blockinst)), parse_arg);
	if (wrote < 0 || exception_check(err))
	{
		return false;
	}

	blockinst_userdata(blockinst) = block_callconstructor(blockinst_block(blockinst), header);
	return true;
}

void blockinst_act(blockinst_t * blockinst, blockact_f callback)
{
	// Sanity check
	{
		if unlikely(blockinst == NULL || callback == NULL)
		{
			return;
		}
	}

	callback(blockinst_userdata(blockinst));
}
