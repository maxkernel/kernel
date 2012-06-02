#include <errno.h>

#include <aul/exception.h>
#include <aul/iterator.h>

#include <array.h>
#include <buffer.h>
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

static inline rategroup_t * rategroup_getrunning()
{
	kthread_t * self = kthread_self();
	if (unlikely(self == NULL))
	{
		return NULL;
	}

	return kthread_getuserdata(self);
}

static bool rategroup_run(kthread_t * thread, void * userdata)
{
	rategroup_t * rg = userdata;

	list_t * pos = NULL;
	list_foreach(pos, &rg->blockinsts)
	{
		rategroup_blockinst_t * rg_blockinst = list_entry(pos, rategroup_blockinst_t, rategroup_list);

		// Handle all the input links
		link_doinputs(&rg_blockinst->ports, &rg_blockinst->blockinst->links);

		// Set up the active cache
		rg->active = rg_blockinst;

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
		rg->active = NULL;

		// Handle all the output links
		link_dooutputs(&rg_blockinst->ports, &rg_blockinst->blockinst->links);
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

		if unlikely(rategroup == NULL || blockinst == NULL)
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
	if (!port_makeblockports(blockinst_getblock(blockinst), &backing->ports, err))
	{
		return false;
	}

	// Add the backing to the rategroup
	list_add(&rategroup->blockinsts, &backing->rategroup_list);

	return true;
}

void rategroup_schedule(rategroup_t * rategroup, trigger_t * trigger, int priority, exception_t ** err)
{
	static int rid = 1;

	string_t name = string_new("Rategroup (%d)", rid++);
	kthread_t * thread = kthread_new(name.string, priority, trigger, rategroup_run, NULL, rategroup, err);
	kthread_schedule(thread);
}

const void * rategroup_input(const char * name)
{
	// Sanity check
	{
		if (unlikely(name == NULL))
		{
			return NULL;
		}
	}


	rategroup_t * rg = rategroup_getrunning();
	if (unlikely(rg == NULL))
	{
		LOGK(LOG_WARN, "Could not get executing rategroup. Invalid operating context!");
		return NULL;
	}

	rategroup_blockinst_t * rg_blockinst = rg->active;
	if (unlikely(rg_blockinst == NULL))
	{
		LOGK(LOG_WARN, "Rategroup is not currently executing a block instance!");
		return NULL;
	}

	port_t * port = port_lookup(&rg_blockinst->ports, meta_input, name);
	if (unlikely(port == NULL))
	{
		LOGK(LOG_WARN, "Could not find input '%s' in block instance!", name);
		return NULL;
	}

	return iobacking_data(port_getbacking(port));
}

void rategroup_output(const char * name, const void * output)
{
	// Sanity check
	{
		if unlikely(name == NULL)
		{
			return;
		}
	}


	rategroup_t * rg = rategroup_getrunning();
	if unlikely(rg == NULL)
	{
		LOGK(LOG_WARN, "Could not get executing rategroup. Invalid operating context!");
		return;
	}

	rategroup_blockinst_t * rg_blockinst = rg->active;
	if unlikely(rg_blockinst == NULL)
	{
		LOGK(LOG_WARN, "Rategroup is not currently executing a block instance!");
		return;
	}

	port_t * port = port_lookup(&rg_blockinst->ports, meta_output, name);
	if unlikely(port == NULL)
	{
		LOGK(LOG_WARN, "Could not find input '%s' in block instance!", name);
		return;
	}

	iobacking_t * backing = port_getbacking(port);
	switch (iobacking_sig(backing))
	{
		case T_BOOLEAN:
		{
			*(bool *)iobacking_data(backing) = *(const bool *)output;
			break;
		}

		case T_INTEGER:
		{
			*(int *)iobacking_data(backing) = *(const int *)output;
			break;
		}

		case T_DOUBLE:
		{
			*(double *)iobacking_data(backing) = *(const double *)output;
			break;
		}

		case T_CHAR:
		{
			*(char *)iobacking_data(backing) = *(const char *)output;
			break;
		}

		case T_STRING:
		{
			strncpy(*(char **)iobacking_data(backing), *(const char **)output, AUL_STRING_MAXLEN - 1);
			break;
		}

		case T_ARRAY_BOOLEAN:
		case T_ARRAY_INTEGER:
		case T_ARRAY_DOUBLE:
		{
			*(array_t *)iobacking_data(backing) = array_dup(*(const array_t *)output);
			break;
		}

		case T_BUFFER:
		{
			*(buffer_t *)iobacking_data(backing) = buffer_dup(*(const buffer_t *)output);
			break;
		}

		default:
		{
			LOGK(LOG_ERR, "Unknown output signature for output '%s'", name);
			break;
		}
	}
}
