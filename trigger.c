#include <unistd.h>
#include <string.h>

#include <aul/common.h>

#include <kernel.h>
#include <kernel-priv.h>


#define MAXIMUM_SLEEP_NANO		50000000	//max sleep 50 milliseconds
#define WARN_NSEC_TOLLERENCE	5000		//warn if clock overshoot by (5 microseconds)

#define VARCLOCK_NAME			"variable trigger"
#define VARCLOCK_KOBJ_NAME		"variable clock trigger"
#define VARCLOCK_BLK_NAME		"varclock"
#define VARCLOCK_BLK_IO			"rate"


static inline void addnanos(struct timespec * val, uint64_t add_nanos)
{
	uint64_t nanos = val->tv_nsec + add_nanos;
	val->tv_nsec = nanos % NANOS_PER_SECOND;
	val->tv_sec += nanos / NANOS_PER_SECOND;
}

static inline int64_t diffnanos(struct timespec * a, struct timespec * b)
{
	int64_t diff = (a->tv_sec - b->tv_sec) * NANOS_PER_SECOND;
	diff += a->tv_nsec - b->tv_nsec;
	return diff;
}

static inline uint64_t hz2nanos(double freq_hz)
{
	return (1.0 / freq_hz) * NANOS_PER_SECOND;
}

static inline struct timespec nanos2timespec(uint64_t nanos)
{
	struct timespec tm;
	memset(&tm, 0, sizeof(struct timespec));
	tm.tv_nsec = nanos % NANOS_PER_SECOND;
	tm.tv_sec = nanos / NANOS_PER_SECOND;

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
	trigger->function = trigfunc;

	return trigger;
}

bool trigger_watch(trigger_t * trigger)
{
	// Sanity check
	{
		if (trigger == NULL)
		{
			// If the trigger is NULL, just return true to indicate that it has been triggered
			return true;
		}
	}

	return trigger->function(trigger);
}

/*------------------- CLOCK -------------------------*/
static char * trigger_infoclock(void * obj)
{
	char * str = "[PLACEHOLDER CLOCK INFO]";
	return strdup(str);
}

static bool trigger_waitclock(trigger_t * trigger)
{
	trigger_clock_t * clk = (void *)trigger;

	if (clk->interval_nsec == 0)
	{
		// Trigger interval is 0 (never trigger), sleep max time and return false
		struct timespec sleeptime = nanos2timespec(MAXIMUM_SLEEP_NANO);
		nanosleep(&sleeptime, NULL);

		return false;
	}


	if (clk->last_trigger.tv_sec == 0 && clk->last_trigger.tv_nsec == 0)
	{
		// Clock hasn't been init yet
		clock_gettime(CLOCK_REALTIME, &clk->last_trigger);
		return true;
	}

	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);

	int64_t diff = diffnanos(&now, &clk->last_trigger);
	if (diff >= clk->interval_nsec)
	{
		if ((diff - WARN_NSEC_TOLLERENCE) > clk->interval_nsec)
		{
			const char * object_name = NULL;
			kobj_getinfo(&clk->trigger.kobject, NULL, &object_name, NULL);

			LOGK(LOG_WARN, "Trigger %s has become unsynchronized (clock overshoot of %" PRIu64 " nanoseconds)", object_name, (diff - clk->interval_nsec));
			clock_gettime(CLOCK_REALTIME, &clk->last_trigger);
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
		// Sleep maximum time
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
}

trigger_clock_t * trigger_newclock(const char * name, double freq_hz)
{
	string_t str = string_new("%s (@ %0.3fHz) clock trigger", name, freq_hz);
	trigger_clock_t * clk = trigger_new(str.string, trigger_infoclock, NULL, trigger_waitclock, sizeof(trigger_clock_t));
	clk->interval_nsec = hz2nanos(freq_hz);

	return clk;
}

/*----------------------- VARCLOCK ------------------------*/
static char * trigger_infovarclock(void * obj)
{
	char * str = "[PLACEHOLDER VARCLOCK INFO]";
	return strdup(str);
}

static bool trigger_waitvarclock(trigger_t * trigger)
{
	trigger_varclock_t * vclk = (void *)trigger;
	trigger_clock_t * clk = &vclk->clock;

	// TODO IMPORTANT - get block input rate!
	//io_beforeblock(clk->blockinst);
	//const double * new_freq_hz = io_doinput(clk->blockinst, VARCLOCK_BLK_IO);

	uint64_t new_interval = clk->interval_nsec; //////new_freq_hz == NULL? clk->interval_nsec : hz2nanos(*new_freq_hz);
	if (new_interval != clk->interval_nsec)
	{
		clk->interval_nsec = new_interval;

		struct timespec now, trigger;
		clock_gettime(CLOCK_REALTIME, &now);
		memcpy(&trigger, &clk->last_trigger, sizeof(struct timespec));

		int64_t diff = diffnanos(&now, &trigger);
		if (diff > clk->interval_nsec)
		{
			memcpy(&clk->last_trigger, &now, sizeof(struct timespec));
			return true;
		}
	}

	return trigger_waitclock(trigger);
}

/*
blockinst_t * trigger_varclock_getblockinst(trigger_t * trigger)
{
	if (strcmp(trigger->kobject.object_name + (strlen(trigger->kobject.object_name) - strlen(VARCLOCK_NAME)), VARCLOCK_NAME) != 0)
	{
		LOGK(LOG_ERR, "KObject '%s' in not a valid varclock trigger", trigger->kobject.object_name);
		return NULL;
	}

	trigger_varclock_t * clk = (trigger_varclock_t *)trigger;
	return clk->blockinst;
}

binput_inst_t * trigger_varclock_getrateinput(trigger_t * trigger)
{
	if (strcmp(trigger->kobject.object_name + (strlen(trigger->kobject.object_name) - strlen(VARCLOCK_NAME)), VARCLOCK_NAME) != 0)
	{
		LOGK(LOG_ERR, "KObject '%s' in not a valid varclock trigger", trigger->kobject.object_name);
		return NULL;
	}

	trigger_varclock_t * clk = (trigger_varclock_t *)trigger;

	list_t * pos;
	binput_inst_t * in = NULL;
	list_foreach(pos, &clk->blockinst->inputs_inst)
	{
		binput_inst_t * test = list_entry(pos, binput_inst_t, blockinst_list);
		if (strcmp(VARCLOCK_BLK_IO, test->input->name) == 0)
		{
			in = test;
			break;
		}
	}

	if (in == NULL)
	{
		LOGK(LOG_WARN, "Variable clock '%s' doesn't have a connected input!", clk->kobject.object_name);
	}

	return in;
}
*/

trigger_varclock_t * trigger_newvarclock(const char * name, double initial_freq_hz)
{
	string_t str = string_new("%s %s", name, VARCLOCK_NAME);
	trigger_varclock_t * clk = trigger_new(str.string, trigger_infovarclock, NULL, trigger_waitvarclock, sizeof(trigger_varclock_t));
	clk->clock.interval_nsec = hz2nanos(initial_freq_hz);
	linklist_init(&clk->links);

	return clk;
}
