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

static bool rategroup_run(kthread_t * thread, void * userdata)
{
	rategroup_t * rg = userdata;

	list_t * pos = NULL;
	list_foreach(pos, &rg->blockinsts)
	{
		rategroup_blockinst_t * rg_blockinst = list_entry(pos, rategroup_blockinst_t, rategroup_list);

		// Handle all the input links
		{
			// TODO IMPORTANT - make this more optimized and better!! It's a piece of shit!
			iobacking_t * lu_inputport(const model_linksymbol_t * symbol)
			{
				const char * name = NULL;
				model_getlinksymbol(symbol, NULL, &name, NULL, NULL);

				list_t * pos = NULL;
				list_foreach(pos, &rg_blockinst->ports)
				{
					port_t * port = list_entry(pos, port_t, port_list);
					if (port_test(port, meta_input, name))
					{
						return port;
					}
				}

				return NULL;
			}

			link_doinputs(&rg_blockinst->blockinst->links, lu_inputport);
		}

		// Set up the active cache
		{
			rg->active = rg_blockinst;
		}

		// Call the onupdate function
		{
			const block_t * block = blockinst_getblock(rg_blockinst->blockinst);

			blockact_f onupdate = NULL;
			block_getonupdate(block, &onupdate);

			if (onupdate != NULL)
			{
				blockinst_act(rg_blockinst->blockinst, onupdate);
			}
		}

		// Clear the active cache
		{
			rg->active = NULL;
		}

		// Handle all the output links
		{
			// TODO IMPORTANT - make this more optimized and better!! It's a piece of shit!
			iobacking_t * lu_outputport(const model_linksymbol_t * symbol)
			{
				const char * name = NULL;
				model_getlinksymbol(symbol, NULL, &name, NULL, NULL);

				list_t * pos = NULL;
				list_foreach(pos, &rg_blockinst->ports)
				{
					port_t * port = list_entry(pos, port_t, port_list);
					if (port_test(port, meta_output, name))
					{
						return port;
					}
				}

				return NULL;
			}

			link_dooutputs(&rg_blockinst->blockinst->links, lu_outputport);
		}
	}

	return true;
}

rategroup_t * rategroup_new(const char * name, const model_linkable_t * linkable)
{
	// Sanity check
	{
		if (name == NULL)
		{
			return NULL;
		}

		if (model_type(model_object(linkable)) != model_rategroup)
		{
			return NULL;
		}
	}

	rategroup_t * rg = kobj_new("Rategroup", name, rategroup_info, rategroup_destroy, sizeof(rategroup_t));
	rg->backing = linkable;
	LIST_INIT(&rg->blockinsts);

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

	rategroup_blockinst_t * backing = malloc(sizeof(rategroup_blockinst_t));
	memset(backing, 0, sizeof(rategroup_blockinst_t));
	backing->blockinst = blockinst;
	portlist_init(&backing->ports);

	// Create the ports
	if (!port_makeblock(blockinst_getblock(blockinst), &backing->ports, err))
	{
		return false;
	}

	// Add the backing to the rategroup
	list_add(&rategroup->blockinsts, &backing->rategroup_list);
}

void rategroup_schedule(rategroup_t * rategroup, trigger_t * trigger, int priority, exception_t ** err)
{
	static int rid = 1;

	string_t name = string_new("Rategroup (%d)", rid++);
	kthread_t * thread = kthread_new(name.string, priority, trigger, rategroup_run, NULL, rategroup, err);
	kthread_schedule(thread);
}
