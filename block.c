#include <errno.h>

#include <aul/exception.h>
#include <aul/iterator.h>

#include <kernel.h>
#include <kernel-priv.h>


static char * block_info(void * block)
{
	char * str = "[PLACEHOLDER BLOCK INFO]";
	return strdup(str);
}

static void block_destroy(void * block)
{
	block_t * blk = block;

	list_t * pos, * n;
	list_foreach_safe(pos, n, &blk->insts)
	{
		blockinst_t * inst = list_entry(pos, blockinst_t, block_list);
		list_remove(&inst->block_list);

		blockinst_act(inst, blk->ondestroy);
		kobj_destroy(&inst->kobject);
	}
}

block_t * block_new(module_t * module, const meta_block_t * backing, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}

		if (module == NULL || backing == NULL)
		{
			exception_set(err, EINVAL, "Invalid arguments!");
			return NULL;
		}
	}

	const char * name = NULL;
	meta_getblock(backing, &name, NULL, NULL, NULL, NULL, NULL);

	const meta_blockcallback_t * onupdate = NULL, * ondestroy = NULL;
	if (!meta_lookupblockcbs(module_getmeta(module), backing, &onupdate, &ondestroy))
	{
		exception_set(err, EFAULT, "Could not lookup meta block callbacks on block %s", name);
		return NULL;
	}

	block_t * blk = kobj_new("Block", name, block_info, block_destroy, sizeof(block_t));
	blk->backing = backing;
	blk->module = module;
	meta_getblockcb(onupdate, NULL, NULL, &blk->onupdate);
	meta_getblockcb(ondestroy, NULL, NULL, &blk->ondestroy);
	LIST_INIT(&blk->insts);

	return blk;
}

void block_getonupdate(const block_t * block, blockact_f * onupdate)
{
	// Sanity check
	{
		if (block == NULL)
		{
			return;
		}
	}

	if (onupdate != NULL)		*onupdate = block->onupdate;
}

void block_addinst(block_t * block, blockinst_t * blockinst)
{
	list_add(&block->insts, &blockinst->block_list);
	kobj_makechild(&block->kobject, &blockinst->kobject);
}

iterator_t block_ioitr(const block_t * block)
{
	// Sanity check
	{
		if (block == NULL)
		{
			return iterator_none();
		}
	}

	const void * ioitr_next(const void * object, void ** itrobject)
	{
		const char * block = object;
		iterator_t * meta_bitr = (iterator_t *)*itrobject;

		while (true)
		{
			const meta_blockio_t * blockio = NULL;
			if (!meta_blockionext(*meta_bitr, &blockio))
			{
				return NULL;
			}

			if (blockio == NULL)
			{
				return NULL;
			}

			const char * block_name = NULL;
			meta_getblockio(blockio, &block_name, NULL, NULL, NULL, NULL);

			if (strcmp(block_name, block) == 0)
			{
				return blockio;
			}
		}
	}

	void ioitr_free(const void * object, void * itrobject)
	{
		iterator_free(*(iterator_t *)itrobject);
		free(itrobject);
	}

	iterator_t * meta_bitr = malloc(sizeof(iterator_t));
	*meta_bitr = meta_blockioitr(module_getmeta(block->module));

	const char * block_name = NULL;
	meta_getblock(block->backing, &block_name, NULL, NULL, NULL, NULL, NULL);

	return iterator_new("block_io", ioitr_next, ioitr_free, block_name, meta_bitr);
}

bool block_ioitrnext(iterator_t itr, const meta_blockio_t ** blockio)
{
	const meta_blockio_t * nextblockio = iterator_next(itr, "block_io");
	if (nextblockio != NULL)
	{
		if (blockio != NULL)		*blockio = nextblockio;
		return true;
	}

	return false;
}
