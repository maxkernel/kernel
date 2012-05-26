#include <errno.h>

#include <aul/exception.h>
#include <aul/iterator.h>

#include <kernel.h>
#include <kernel-priv.h>

extern list_t rategroups;


static char * rategroup_info(void * rategroup)
{
	char * str = "[PLACEHOLDER RATEGROUP INFO]";
	return strdup(str);
}

static void rategroup_destroy(void * rategroup)
{
	rategroup_t * rg = rategroup;

	// TODO - destroy the rategroup
}

static riobacking_t * rategroup_newbacking(blockinst_t * blockinst, const char * name, meta_iotype_t type, char sig, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}

		if (strlen(name) >= MODEL_SIZE_NAME)
		{
			exception_set(err, EINVAL, "Rategroup backing name too long: %s", name);
			return NULL;
		}
	}

	biobacking_t * bbacking = link_newbacking(sig, err);
	if (bbacking == NULL || exception_check(err))
	{
		return NULL;
	}

	riobacking_t * backing = malloc(sizeof(riobacking_t));
	memset(backing, 0, sizeof(riobacking_t));
	backing->blockinst = blockinst;
	backing->type = type;
	strcpy(backing->name, name);
	backing->backing = bbacking;

	return backing;
}

static bool rategroup_run(kthread_t * thread, void * userdata)
{
	rategroup_t * rg = userdata;


	for (size_t i = 0; rg->group[i] != NULL; i++)
	{
		blockinst_t * activeinst = NULL;

		// Set up the active cache
		{
			activeinst = rg->active = rg->group[i];
			rg->activeriobackings = NULL;

			// TODO - make this more efficient (we know the list is sorted!)
			list_t * pos = NULL;
			list_foreach(pos, &rg->riobackings)
			{
				riobacking_t * backing = list_entry(pos, riobacking_t, rategroup_list);
				if (backing->blockinst == activeinst)
				{
					rg->activeriobackings = pos;
					break;
				}
			}
		}
		// TODO IMPORTANT - do io_before

		// Call the onupdate function
		{
			const block_t * block = blockinst_getblock(activeinst);

			blockact_f onupdate = NULL;
			block_getonupdate(block, &onupdate);

			if (onupdate != NULL)
			{
				blockinst_act(activeinst, onupdate);
			}
		}

		// TODO IMPORTANT - do io_after
	}

	rg->active = NULL;
	rg->activeriobackings = NULL;

	return true;
}

rategroup_t * rategroup_new(const char * name)
{
	// Sanity check
	{
		if (name == NULL)
		{
			return NULL;
		}
	}

	rategroup_t * rg = kobj_new("Rategroup", name, rategroup_info, rategroup_destroy, sizeof(rategroup_t));
	LIST_INIT(&rg->riobackings);

	list_add(&rategroups, &rg->global_list);
	return rg;
}

bool rategroup_addblockinst(rategroup_t * rategroup, blockinst_t * blockinst, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return false;
		}

		if (rategroup == NULL || blockinst == NULL)
		{
			exception_set(err, EINVAL, "Invalid arguments!");
			return false;
		}
	}

	const block_t * block = blockinst_getblock(blockinst);

	for (size_t i = 0; i < MODEL_MAX_RATEGROUPELEMS; i++)
	{
		if (rategroup->group[i] == NULL)
		{
			// Create the localio
			{
				iterator_t ioitr = block_ioitr(block);
				const meta_blockio_t * blockio = NULL;
				while (block_ioitrnext(ioitr, &blockio))
				{
					const char * name = NULL;
					meta_iotype_t type = meta_unknownio;
					char sig = '\0';
					meta_getblockio(blockio, NULL, &name, &type, &sig, NULL);

					riobacking_t * rbacking = rategroup_newbacking(blockinst, name, type, sig, err);
					if (rbacking == NULL || exception_check(err))
					{
						return false;
					}

					list_add(&rategroup->riobackings, &rbacking->rategroup_list);
				}
				iterator_free(ioitr);
			}

			// Add the blockinst to the rategroup
			rategroup->group[i] = blockinst;

			return true;
		}
	}

	// Got here, no free space in rategroup!
	exception_set(err, ENOMEM, "Rategroup is full! (MAX = %d)", MODEL_MAX_RATEGROUPELEMS);
	return false;
}

void rategroup_schedule(rategroup_t * rategroup, trigger_t * trigger, int priority, exception_t ** err)
{
	static int rid = 1;

	string_t name = string_new("Rategroup (%d)", rid++);
	kthread_t * thread = kthread_new(name.string, priority, trigger, rategroup_run, NULL, rategroup, err);
	kthread_schedule(thread);
}
