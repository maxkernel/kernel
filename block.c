#include <stdio.h>
#include <errno.h>

#include <aul/exception.h>
#include <aul/iterator.h>

#include <kernel.h>
#include <kernel-priv.h>


static ssize_t block_desc(const kobject_t * object, char * buffer, size_t length)
{
	const block_t * blk = (const block_t *)object;
	return snprintf(buffer, length, "{ 'name': '%s', 'description': '%s', 'signature': '%s', 'signature_description': '%s', 'module_id': '%#x' }", blk->name, ser_string(blk->description), ser_string(blk->newsignature), ser_string(blk->newdescription), kobj_id(kobj_cast(blk->module)));
}

static void block_destroy(kobject_t * object)
{
	block_t * blk = (block_t *)object;

	list_t * pos = NULL, * n = NULL;
	list_foreach_safe(pos, n, &blk->insts)
	{
		blockinst_t * inst = list_entry(pos, blockinst_t, block_list);
		list_remove(&inst->block_list);
		kobj_destroy(kobj_cast(inst));
	}

	function_free(blk->new);
	free(blk->name);
	free(blk->description);
	free(blk->newsignature);
	free(blk->newdescription);
}

block_t * block_new(module_t * module, const meta_block_t * block, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(module == NULL || block == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}

	const meta_t * meta = module_meta(module);
	if (meta == NULL)
	{
		exception_set(err, EFAULT, "Could not get meta backing for module backing");
		return NULL;
	}

	const char * name = NULL, * desc = NULL, * new_sig = NULL, * new_desc;
	meta_callback_f new = NULL;
	meta_getblock(block, &name, &desc, NULL, &new_sig, &new_desc, &new);
	if (new_sig == NULL || (strlen(new_sig) > 0 && new == NULL))
	{
		exception_set(err, EINVAL, "Bad constructor defined for block '%s'", name);
		return NULL;
	}

	const meta_blockcallback_t * onupdate = NULL, * ondestroy = NULL;
	meta_lookupblockcbs(meta, block, &onupdate, &ondestroy);

	meta_callback_vp_f onupdate_cb = NULL, ondestroy_cb = NULL;
	meta_getblockcb(onupdate, NULL, NULL, &onupdate_cb);
	meta_getblockcb(ondestroy, NULL, NULL, &ondestroy_cb);

	if (onupdate_cb == NULL)
	{
		exception_set(err, EINVAL, "Block '%s' update function not defined!", name);
		return NULL;
	}

	string_t newffi_sig = string_new("%c:%s", T_POINTER, new_sig);
	ffi_function_t * newffi = function_build(new, newffi_sig.string, err);
	if (newffi == NULL || exception_check(err))
	{
		return NULL;
	}

	block_t * blk = kobj_new("Block", name, block_desc, block_destroy, sizeof(block_t));
	blk->module = module;
	blk->name = strdup(name);
	blk->description = strdup(desc);
	blk->newsignature = strdup(new_sig);
	blk->newdescription = strdup(new_desc);
	blk->new = newffi;
	blk->onupdate = onupdate_cb;
	blk->ondestroy = ondestroy_cb;
	list_init(&blk->insts);

	return blk;
}

void block_add(block_t * block, blockinst_t * blockinst)
{
	list_add(&block->insts, &blockinst->block_list);
	kobj_makechild(kobj_cast(block), kobj_cast(blockinst));
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

	if (block->new == NULL)
	{
		// No constructor defined, just return NULL
		return NULL;
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
			if (!meta_nextblockio(*meta_bitr, &blockio))
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
	*meta_bitr = meta_itrblockio(module_meta(block->module));

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
