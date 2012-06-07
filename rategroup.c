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
	if unlikely(self == NULL)
	{
		return NULL;
	}

	return kthread_userdata(self);
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
			blockact_f onupdate = block_cbupdate(blockinst_block(rg_blockinst->blockinst));
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

rategroup_t * rategroup_new(const model_linkable_t * linkable, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(model_type(model_object(linkable)) != model_rategroup)
		{
			exception_set(err, EINVAL, "Bad arguments (not a rategroup linkable)");
			return NULL;
		}
	}

	const char * name = NULL;
	double hertz = 0;
	model_getrategroup(linkable, &name, &hertz);

	string_t trigger_name = string_new("%s trigger", name);
	trigger_varclock_t * t = trigger_newvarclock(trigger_name.string, hertz);

	rategroup_t * rg = kobj_new("Rategroup", name, rategroup_info, rategroup_destroy, sizeof(rategroup_t));
	rg->trigger = trigger_cast(t);
	rg->name = strdup(name);
	LIST_INIT(&rg->blockinsts);

	list_add(&rategroups, &rg->global_list);
	return rg;
}

bool rategroup_addblockinst(rategroup_t * rategroup, blockinst_t * blockinst, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
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
	if (!port_makeblockports(blockinst_block(blockinst), &backing->ports, err))
	{
		return false;
	}

	// Add the backing to the rategroup
	list_add(&rategroup->blockinsts, &backing->rategroup_list);

	return true;
}

bool rategroup_schedule(rategroup_t * rategroup, int priority, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(rategroup == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}
	}

	string_t name = string_new("%s thread", rategroup->name);
	kthread_t * thread = kthread_new(name.string, priority, rategroup->trigger, rategroup_run, NULL, rategroup, err);
	if (thread == NULL || exception_check(err))
	{
		return false;
	}

	kthread_schedule(thread);
	return true;
}

const void * rategroup_input(const char * name)
{
	// Sanity check
	{
		if unlikely(name == NULL)
		{
			return NULL;
		}
	}


	rategroup_t * rg = rategroup_getrunning();
	if unlikely(rg == NULL)
	{
		LOGK(LOG_WARN, "Could not get executing rategroup. Invalid operating context!");
		return NULL;
	}

	rategroup_blockinst_t * rg_blockinst = rg->active;
	if unlikely(rg_blockinst == NULL)
	{
		LOGK(LOG_WARN, "Rategroup is not currently executing a block instance!");
		return NULL;
	}

	port_t * port = port_lookup(&rg_blockinst->ports, meta_input, name);
	if unlikely(port == NULL)
	{
		LOGK(LOG_WARN, "Could not find input '%s' in block instance!", name);
		return NULL;
	}

	iobacking_t * backing = port_iobacking(port);
	if (iobacking_isnull(backing))
	{
		return NULL;
	}

	return iobacking_data(backing);
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
		LOGK(LOG_WARN, "Could not find output '%s' in block instance!", name);
		return;
	}

	iobacking_copy(port_iobacking(port), output);
}
