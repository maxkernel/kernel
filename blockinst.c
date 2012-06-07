#include <string.h>

#include <aul/iterator.h>
#include <aul/parse.h>

#include <method.h>
#include <serialize.h>
#include <kernel.h>
#include <kernel-priv.h>


static char * blockinst_info(void * blockinst)
{
	char * str = "[PLACEHOLDER BLOCKINST INFO]";
	return strdup(str);
}

static void blockinst_destroy(void * blockinst)
{
	blockinst_t * blkinst = blockinst;

	// TODO IMPORTANT - call the ondestroy callback function of the block instance
}

blockinst_t * blockinst_new(block_t * block, const char * name, const char * const * args, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(block == NULL || name == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}

	blockinst_t * inst = kobj_new("Block Instance", name, blockinst_info, blockinst_destroy, sizeof(blockinst_t));
	linklist_init(&inst->links);
	inst->block = block;
	inst->name = strdup(name);
	inst->args = args;			// TODO - do some sort of strcpy here!

	block_add(block, inst);
	return inst;
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
				double v = parse_bool(blockinst->args[index], err);
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
				size_t len = strlen(blockinst->args[index]);
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
	ssize_t wrote = serialize_2array_fromcb_wheader(header, BLOCKINST_BUFFERMAX, err, block_newsig(blockinst_block(blockinst)), parse_arg);
	if (wrote < 0 || exception_check(err))
	{
		// TODO IMPORTANT - fucking finish me!
	}

	return false;
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

	callback(blockinst->userdata);
}
