#include <aul/iterator.h>

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

blockinst_t * blockinst_newempty(const char * name)
{
	blockinst_t * inst = kobj_new("Block Instance", name, blockinst_info, blockinst_destroy, sizeof(blockinst_t));
	LIST_INIT(&inst->inputs);
	LIST_INIT(&inst->outputs);

	return inst;
}

blockinst_t * blockinst_new(block_t * block, const model_linkable_t * backing, const char * name)
{
	blockinst_t * inst = blockinst_newempty(name);
	inst->block = block;
	inst->backing = backing;

	block_addinst(block, inst);

	return inst;
}

const block_t * blockinst_getblock(const blockinst_t * blockinst)
{
	// Sanity check
	{
		if (blockinst == NULL)
		{
			return NULL;
		}
	}

	return blockinst->block;
}

void blockinst_act(blockinst_t * blockinst, blockact_f callback)
{
	// Sanity check
	{
		if (blockinst == NULL || callback == NULL)
		{
			return;
		}
	}

	callback(blockinst->userdata);
}
