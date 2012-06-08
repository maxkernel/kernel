#include <errno.h>

#include <aul/exception.h>
#include <aul/iterator.h>

#include <kernel.h>
#include <kernel-priv.h>


static char * block_info(void * object)
{
	unused(object);

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

block_t * block_new(module_t * module, const char * name, const char * newsig, blockconstructor_f new, blockact_f onupdate, blockact_f ondestroy, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(module == NULL || name == NULL || newsig == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}

		if unlikely(strlen(newsig) != 0 && new == NULL)
		{
			exception_set(err, EINVAL, "Bad constructor for block '%s'", name);
			return NULL;
		}
	}

	string_t newffi_sig = string_new("%c:%s", T_POINTER, newsig);
	ffi_function_t * newffi = function_build(new, newffi_sig.string, err);
	if (newffi == NULL || exception_check(err))
	{
		return NULL;
	}

	/*
	const char * name = NULL;
	meta_getblock(backing, &name, NULL, NULL, NULL, NULL, NULL);

	const meta_blockcallback_t * onupdate = NULL, * ondestroy = NULL;
	if (!meta_lookupblockcbs(module_getmeta(module), backing, &onupdate, &ondestroy))
	{
		exception_set(err, EFAULT, "Could not lookup meta block callbacks on block %s", name);
		return NULL;
	}
	*/

	block_t * blk = kobj_new("Block", name, block_info, block_destroy, sizeof(block_t));
	blk->module = module;
	blk->name = strdup(name);
	blk->newsig = strdup(newsig);
	blk->new = newffi;
	blk->onupdate = onupdate;
	blk->ondestroy = ondestroy;
	//meta_getblockcb(onupdate, NULL, NULL, &blk->onupdate);
	//meta_getblockcb(ondestroy, NULL, NULL, &blk->ondestroy);
	LIST_INIT(&blk->insts);

	return blk;
}

void block_add(block_t * block, blockinst_t * blockinst)
{
	list_add(&block->insts, &blockinst->block_list);
	kobj_makechild(&block->kobject, &blockinst->kobject);
}

void * block_callconstructor(block_t * block, void ** args)
{
	// Sanity check
	{
		if unlikely(block == NULL)
		{
			return NULL;
		}
	}

	void * ret = NULL;
	function_call(block->new, &ret, args);

	return ret;
}

bool block_iolookup(const block_t * block, const char * ioname, meta_iotype_t iotype, char * io_sig, const char ** io_desc)
{
	// Sanity check
	{
		if unlikely(block == NULL || ioname == NULL)
		{
			return false;
		}
	}

	const meta_block_t * meta_block = NULL;
	if (!meta_lookupblock(module_meta(block->module), block->name, &meta_block))
	{
		return false;
	}

	const meta_blockio_t * meta_blockio = NULL;
	if (!meta_lookupblockio(module_meta(block->module), meta_block, ioname, iotype, &meta_blockio))
	{
		return false;
	}

	meta_getblockio(meta_blockio, NULL, NULL, NULL, io_sig, io_desc);
	return true;
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
		unused(object);

		iterator_free(*(iterator_t *)itrobject);
		free(itrobject);
	}

	iterator_t * meta_bitr = malloc(sizeof(iterator_t));
	*meta_bitr = meta_blockioitr(module_meta(block->module));

	return iterator_new("block_io", ioitr_next, ioitr_free, block->name, meta_bitr);
}

bool block_ionext(iterator_t itr, const meta_blockio_t ** blockio)
{
	const meta_blockio_t * nextblockio = iterator_next(itr, "block_io");
	if (nextblockio != NULL)
	{
		if (blockio != NULL)		*blockio = nextblockio;
		return true;
	}

	return false;
}
