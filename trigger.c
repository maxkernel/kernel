#include <unistd.h>
#include <string.h>

#include "kernel.h"
#include "kernel-priv.h"

#define NANO_PER_SEC			1000000000LL

#define MAXIMUM_SLEEP_NANO		50000000	//max sleep 50 milliseconds
#define WARN_NSEC_TOLLERENCE	5000		//warn if clock overshoot by (5 microseconds)

#define CLOCK_NAME				"clock trigger"

#define VARCLOCK_NAME			"variable trigger"
#define VARCLOCK_KOBJ_NAME		"variable clock trigger"
#define VARCLOCK_BLK_NAME		"varclock"
#define VARCLOCK_BLK_IO			"rate"

#define true_NAME				"true trigger"


extern module_t * kernel_module;

#define gettime(ptr)			clock_gettime(CLOCK_REALTIME, ptr)

static inline void addnanos(struct timespec * val, uint64_t add_nanos)
{
	uint64_t nanos = val->tv_nsec + add_nanos;
	val->tv_nsec = nanos % NANO_PER_SEC;
	val->tv_sec += nanos / NANO_PER_SEC;
}

static inline int64_t diffnanos(struct timespec * a, struct timespec * b)
{
	int64_t diff = (a->tv_sec - b->tv_sec) * NANO_PER_SEC;
	diff += a->tv_nsec - b->tv_nsec;
	return diff;
}

static inline uint64_t hz2nanos(double freq_hz)
{
	return (1.0 / freq_hz) * NANO_PER_SEC;
}

static inline struct timespec nanos2timespec(uint64_t nanos)
{
	struct timespec tm;
	ZERO(tm);
	tm.tv_nsec = nanos % NANO_PER_SEC;
	tm.tv_sec = nanos / NANO_PER_SEC;
	return tm;
}

void * trigger_new(const char * name, info_f info, destructor_f destructor, trigger_f trigfunc, size_t malloc_size)
{
	if (malloc_size < sizeof(trigger_f))
	{
		LOGK(LOG_FATAL, "Size of new trigger is too small: %zu", malloc_size);
		//will exit
	}

	trigger_t * trigger = kobj_new("Trigger", name, info, destructor, malloc_size);
	trigger->func = trigfunc;

	return trigger;
}

/*------------------- CLOCK -------------------------*/
static char * trigger_infoclock(void * obj)
{
	char * str = "[PLACEHOLDER CLOCK INFO]";
	return strdup(str);
}

static bool trigger_waitclock(void * object)
{
	trigger_clock_t * clk = object;

	if (clk->interval_nsec == 0)
	{
		//trigger interval is 0 (never trigger), sleep max time and return false
		struct timespec sleeptime = nanos2timespec(MAXIMUM_SLEEP_NANO);
		nanosleep(&sleeptime, NULL);

		return false;
	}


	if (clk->last_trigger.tv_sec == 0 && clk->last_trigger.tv_nsec == 0)
	{
		//clock hasn't been init yet
		gettime(&clk->last_trigger);
		return true;
	}

	struct timespec now;
	gettime(&now);

	int64_t diff = diffnanos(&now, &clk->last_trigger);
	//LOG(LOG_INFO, "DIFF %lld, INTERVAL %lld", diff, clk->interval_nsec);
	if (diff >= clk->interval_nsec)
	{
		if ((diff - WARN_NSEC_TOLLERENCE) > clk->interval_nsec)
		{
			LOGK(LOG_WARN, "Trigger %s has become unsynchronized (clock overshoot of %" PRIu64 " nanoseconds)", clk->kobject.obj_name, (diff - clk->interval_nsec));
			gettime(&clk->last_trigger);
		}
		else
		{
			addnanos(&clk->last_trigger, clk->interval_nsec);
		}

		return true;
	}

	uint64_t nanosleft = clk->interval_nsec - diff;
	if (nanosleft >= MAXIMUM_SLEEP_NANO)
	{
		//sleep maximum time
		struct timespec sleeptime = nanos2timespec(MAXIMUM_SLEEP_NANO);
		nanosleep(&sleeptime, NULL);

		return false;
	}
	else
	{
		struct timespec sleeptime = nanos2timespec(nanosleft);
		nanosleep(&sleeptime, NULL);

		addnanos(&clk->last_trigger, clk->interval_nsec);
		return true;
	}



/*
	if (clk->next_trigger == NULL)
	{
		//first execution - construct GTimeVal and return, don't wait

		clk->next_trigger = g_malloc0(sizeof(GTimeVal));
		g_get_current_time(clk->next_trigger);

		long useconds = (long)(1.0 / clk->freq_hz * 1000000.0);
		g_time_val_add(clk->next_trigger, useconds);
	}

	GTimeVal now;
	g_get_current_time(&now);
	long diff = (clk->next_trigger->tv_sec - now.tv_sec) * 1000000 + (clk->next_trigger->tv_usec - now.tv_usec);

	if (diff < 0)
	{
		if (diff < -WARN_USEC_TOLLERENCE)
		{
			LOGK(LOG_WARN, "Trigger %s updating at %f Hz had become unsynchronized (clock overshoot of %ld microseconds)", clk->kobject.obj_name, clk->freq_hz, -diff);

			g_free(clk->next_trigger);
			clk->next_trigger = NULL;
		}

		return true;
	}

	if (diff > MAXIMUM_SLEEP)
	{
		usleep(MAXIMUM_SLEEP);
		return false;
	}

	//if we're here, diff is less than MAXIMUM_SLEEP
	//just sleep it off
	usleep(diff);

	//increment the next_trigger timer
	long useconds = (long)(1.0 / clk->freq_hz * 1000000.0);
	g_time_val_add(clk->next_trigger, useconds);
*/
//	return true;
}

trigger_t * trigger_newclock(const char * name, double freq_hz)
{
	string_t str = string_new("%s (@ %0.3fHz) " CLOCK_NAME, name, freq_hz);
	trigger_clock_t * clk = trigger_new(string_copy(&str), trigger_infoclock, NULL, trigger_waitclock, sizeof(trigger_clock_t));
	clk->interval_nsec = hz2nanos(freq_hz);

	return (trigger_t *)clk;
}

/*----------------------- VARCLOCK ------------------------*/
static char * trigger_infovarclock(void * obj)
{
	char * str = "[PLACEHOLDER VARCLOCK INFO]";
	return strdup(str);
}

static bool trigger_waitvarclock(void * object)
{
	trigger_varclock_t * clk = object;

	io_beforeblock(clk->block_inst);
	const double * new_freq_hz = io_doinput(clk->block_inst, VARCLOCK_BLK_IO);

	uint64_t new_interval = new_freq_hz == NULL? clk->interval_nsec : hz2nanos(*new_freq_hz);
	if (new_interval != clk->interval_nsec)
	{
		clk->interval_nsec = new_interval;

		struct timespec now, trigger;
		gettime(&now);
		memcpy(&trigger, &clk->last_trigger, sizeof(struct timespec));

		int64_t diff = diffnanos(&now, &trigger);
		if (diff > clk->interval_nsec)
		{
			memcpy(&clk->last_trigger, &now, sizeof(struct timespec));
			return true;
		}
	}

	return trigger_waitclock(clk);
/*
	io_beforeblock(clk->block_inst);
	const double * new_freq_hz_ptr = io_doinput(clk->block_inst, VARCLOCK_IO);
	double new_freq_hz = clk->freq_hz;
	if (new_freq_hz_ptr != NULL)
	{
		new_freq_hz = *new_freq_hz_ptr;
	}

	if (new_freq_hz == 0.0)
	{
		clk->freq_hz = 0.0;
		g_free(clk->next_trigger);
		clk->next_trigger = NULL;

		usleep(MAXIMUM_SLEEP);
		return false;
	}

	if (clk->next_trigger == NULL)
	{
		//hasn't run yet
		clk->freq_hz = new_freq_hz;

		//update the io
		io_dooutput(clk->block_inst, VARCLOCK_IO, &clk->freq_hz, true);
		io_afterblock(clk->block_inst);

		return trigger_waitclock(object);
	}
	else if (new_freq_hz == clk->freq_hz)
	{
		//nothing has changed
		return trigger_waitclock(object);
	}
	else
	{
		//the rate has changed
		long old_useconds = (long)(1.0 / clk->freq_hz * 1000000.0);
		long new_useconds = (long)(1.0 / new_freq_hz * 1000000.0);
		g_time_val_add(clk->next_trigger, -old_useconds+new_useconds);

		clk->freq_hz = new_freq_hz;

		//update the io
		io_dooutput(clk->block_inst, VARCLOCK_IO, &clk->freq_hz, true);
		io_afterblock(clk->block_inst);

		GTimeVal * now = g_malloc0(sizeof(GTimeVal));
		g_get_current_time(now);
		long diff = (clk->next_trigger->tv_sec - now->tv_sec) * 1000000 + (clk->next_trigger->tv_usec - now->tv_usec);

		if (diff < 0)
		{
			//we should have updated long ago if we went by new freq, set up the next timeval starting now and trigger immediately
			g_time_val_add(now, new_useconds);
			g_free(clk->next_trigger);
			clk->next_trigger = now;

			return true;
		}
		else
		{
			g_free(now);
			return trigger_waitclock(object);
		}
	}
*/
}

block_inst_t * trigger_varclock_getblockinst(trigger_t * trigger)
{
	if (strcmp(trigger->kobject.obj_name + (strlen(trigger->kobject.obj_name) - strlen(VARCLOCK_NAME)), VARCLOCK_NAME) != 0)
	{
		LOGK(LOG_ERR, "KObject '%s' in not a valid varclock trigger", trigger->kobject.obj_name);
		return NULL;
	}

	trigger_varclock_t * clk = (trigger_varclock_t *)trigger;
	return clk->block_inst;
}

binput_inst_t * trigger_varclock_getrateinput(trigger_t * trigger)
{
	if (strcmp(trigger->kobject.obj_name + (strlen(trigger->kobject.obj_name) - strlen(VARCLOCK_NAME)), VARCLOCK_NAME) != 0)
	{
		LOGK(LOG_ERR, "KObject '%s' in not a valid varclock trigger", trigger->kobject.obj_name);
		return NULL;
	}

	trigger_varclock_t * clk = (trigger_varclock_t *)trigger;

	list_t * pos;
	binput_inst_t * in = NULL;
	list_foreach(pos, &clk->block_inst->inputs_inst)
	{
		binput_inst_t * test = list_entry(pos, binput_inst_t, block_inst_list);
		if (strcmp(VARCLOCK_BLK_IO, test->input->name) == 0)
		{
			in = test;
			break;
		}
	}

	if (in == NULL)
	{
		LOGK(LOG_WARN, "Variable clock '%s' doesn't have a connected input!", clk->kobject.obj_name);
	}

	return in;
}

trigger_t * trigger_newvarclock(const char * name, double initial_freq_hz)
{
	list_t * pos;
	block_t * blk = NULL;

	list_foreach(pos, &kernel_module->blocks)
	{
		block_t * test = list_entry(pos, block_t, module_list);
		if (strcmp(VARCLOCK_BLK_NAME, test->name) == 0)
		{
			// We found the varclock block that
			blk = test;
			break;
		}
	}

	if (blk == NULL)
	{
		// The varclock block doesn't exist, create it

		blk = kobj_new("Block", strdup(VARCLOCK_KOBJ_NAME), io_blockinfo, io_blockfree, sizeof(block_t));
		blk->name = strdup(VARCLOCK_BLK_NAME);
		blk->desc = strdup("Variable clock trigger block");
		blk->module = kernel_module;
		LIST_INIT(&blk->inputs);
		LIST_INIT(&blk->outputs);

		bio_t * in = malloc0(sizeof(bio_t));
		bio_t * out = malloc0(sizeof(bio_t));

		in->block = blk;
		in->name = strdup(VARCLOCK_BLK_IO);
		in->sig = T_DOUBLE;
		in->desc = strdup("Update rate (in Hz) to execute the rategroup");

		out->block = blk;
		out->name = strdup(VARCLOCK_BLK_IO);
		out->sig = T_DOUBLE;
		out->desc = strdup("The update rate (in Hz) that the rategroup is executed at");

		list_add(&blk->inputs, &in->block_list);
		list_add(&blk->outputs, &out->block_list);

		list_add(&kernel_module->blocks, &blk->module_list);
	}

	string_t str = string_new("%s " VARCLOCK_NAME, name);
	trigger_varclock_t * clk = trigger_new(string_copy(&str), trigger_infovarclock, NULL, trigger_waitvarclock, sizeof(trigger_varclock_t));
	clk->interval_nsec = hz2nanos(initial_freq_hz);
	clk->block_inst = io_newblock(blk, NULL);

	return (trigger_t *)clk;
}

/* ---------------------------- TRIGGER true ------------------------------- */
static char * trigger_infotrue(void * obj)
{
	char * str = "[PLACEHOLDER TRIGGER INFO]";
	return strdup(str);
}

static bool trigger_waittrue(void * object)
{
	return true;
}

trigger_t * trigger_newtrue(const char * name)
{
	string_t str = string_new("%s " true_NAME, name);
	trigger_t * trig = trigger_new(string_copy(&str), trigger_infotrue, NULL, trigger_waittrue, sizeof(trigger_t));
	return trig;
}
