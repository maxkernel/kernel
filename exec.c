#include <string.h>

#include "kernel.h"
#include "kernel-priv.h"

#define EXEC_GROUP_NAME		"rungroup"
#define EXEC_FUNCTION_NAME	"function"

/*
static void exec_doclock(gpointer data)
{
	exec_clk_t * eclk = data;

	//just execute the function
	(*eclk->clock_func)();
}

static void exec_doblock(gpointer data)
{
	exec_block_t * blk = data;

	GSList * next = blk->inst_list;
	blk->on_inst = 0;

	while (next != NULL)
	{
		block_inst_t * blk_inst = next->data;
/ *
		io_beforeblock(blk_inst);
		blk_inst->block->onupdate(blk_inst->userdata);
		io_afterblock(blk_inst);
* /
		blk->on_inst++;
		next = next->next;
	}

	blk->on_inst = -1;
}
*/

static char * exec_inforungroup(void * obj)
{
	char * str = "[PLACEHOLDER INFO RUNGROUP]";
	return strdup(str);
}

static char * exec_infofunction(void * obj)
{
	char * str = "[PLACEHOLDER INFO FUNCTION]";
	return strdup(str);
}

static void exec_dorungroup(void * object)
{
	rungroup_t * rungrp = object;
	rungrp->onitem = 0;
	runnable_t * item = NULL;

	while ((item = rungrp->list[rungrp->onitem]) != NULL)
	{
		item->runfunc(item);
		rungrp->onitem++;
	}

	rungrp->onitem = 0;
}

static void exec_dofunction(void * object)
{
	runfunction_t * rfunc = object;
	rfunc->dorunfunc(rfunc->userdata);
}

static void exec_stopfunction(void * object)
{
	runfunction_t * rfunc = object;
	if (rfunc->dostopfunc != NULL)
	{
		rfunc->dostopfunc(rfunc->userdata);
	}
}

static void exec_freerungroup(void * object)
{
	rungroup_t * grp = object;
	g_free(grp->list);
}

void * exec_new(const char * name, info_f info, destructor_f destructor, handler_f runfunc, handler_f stopfunc, size_t malloc_size)
{
	if (malloc_size < sizeof(runnable_t))
	{
		LOGK(LOG_FATAL, "Runnable (runnable_t) size is too small: %zu", malloc_size);
		//will exit
	}

	runnable_t * runnable = kobj_new("Runnable", name, info, destructor, malloc_size);
	runnable->runfunc = runfunc;
	runnable->stopfunc = stopfunc;

	return runnable;
}

runnable_t * exec_newrungroup(const char * name, runnable_t ** runnables)
{
	String nm = string_new("%s " EXEC_GROUP_NAME, name);
	rungroup_t * grp = exec_new(string_copy(&nm), exec_inforungroup, exec_freerungroup, exec_dorungroup, NULL, sizeof(rungroup_t));
	grp->onitem = 0;
	grp->list = runnables;

	return (runnable_t *)grp;
}

runnable_t * exec_newfunction(const char * name, handler_f runfunc, handler_f stopfunc, void * userdata)
{
	String nm = string_new("%s " EXEC_FUNCTION_NAME, name);
	runfunction_t * rfunc = exec_new(string_copy(&nm), exec_infofunction, NULL, exec_dofunction, exec_stopfunction, sizeof(runfunction_t));
	rfunc->dorunfunc = runfunc;
	rfunc->dostopfunc = stopfunc;
	rfunc->userdata = userdata;

	return (runnable_t *)rfunc;
}

/*
exec_f * exec_newclock(clock_f func)
{
	exec_clk_t * eclk = exec_new(exec_doclock, sizeof(exec_clk_t));
	eclk->clock_func = func;

	return (exec_f *)eclk;
}

exec_f * exec_newblock(block_inst_t ** blocks, gsize numblocks)
{
	exec_block_t * blk = exec_new(exec_doblock, sizeof(exec_block_t));
	blk->on_inst = -1;

	gsize i=0;
	for (; i<numblocks; i++)
	{
		if (blocks[i] == NULL)
		{
			LOGK(LOG_FATAL, "Block[%d] in exec_newblock is null!", i);
			//will exit
		}

		blk->inst_list = g_slist_append(blk->inst_list, blocks[i]);
	}

	return (exec_f *)blk;
}
*/

static runnable_t * __exec_getcurrent(runnable_t * run)
{
	if (strsuffix(run->kobject.obj_name, EXEC_GROUP_NAME))
	{
		rungroup_t * grp = (rungroup_t *)run;
		return __exec_getcurrent(grp->list[grp->onitem]);	//handle nested rungroups
	}

	return run;
}

runnable_t * exec_getcurrent()
{
	runnable_t * run = kthread_getrunnable(kthread_self());
	if (run == NULL)
	{
		LOG(LOG_INFO, "EXEC_CURRENT=NULL");
		return NULL;
	}

	return __exec_getcurrent(run);
}

/*
gboolean exec_isblockinst(exec_f * exec)
{
	if (exec == NULL)
		return false;

	return *exec == exec_doblock;
}

block_inst_t * exec_getblockinst(exec_f * exec)
{
	if (!exec_isblockinst(exec))
	{
		return NULL;
	}

	exec_block_t * blk = (exec_block_t *)exec;
	if (blk->on_inst == -1)
	{
		return NULL;
	}

	return g_slist_nth(blk->inst_list, blk->on_inst)->data;
}
*/
