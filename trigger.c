#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <aul/common.h>

#include <kernel.h>
#include <kernel-priv.h>


#define MAXIMUM_SLEEP_NANO		50000000	// Max sleep 50 milliseconds
#define WARN_NSEC_TOLLERENCE	5000		// Warn if clock overshoot by (5 microseconds)

#define VARCLOCK_RATE_PORT		"rate"


static inline void addnanos(struct timespec * val, uint64_t add_nanos)
{
	uint64_t nanos = val->tv_nsec + add_nanos;
	val->tv_nsec = nanos % NANOS_PER_SECOND;
	val->tv_sec += nanos / NANOS_PER_SECOND;
}

static inline uint64_t diffnanos(struct timespec * a, struct timespec * b)
{
	int64_t diff = (a->tv_sec - b->tv_sec) * NANOS_PER_SECOND;
	diff += a->tv_nsec - b->tv_nsec;
	return (uint64_t)diff;
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

void * trigger_new(const char * name, desc_f info, destructor_f destructor, trigger_f trigfunc, size_t malloc_size)
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

// ------------------- CLOCK -------------------------
static ssize_t trigger_descclock(const kobject_t * object, char * buffer, size_t length)
{
	const trigger_clock_t * clk = (const trigger_clock_t *)object;
	return snprintf(buffer, length, "{ 'frequency': %f }", clk->freq_hz);
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

	uint64_t diff = diffnanos(&now, &clk->last_trigger);
	if (diff >= clk->interval_nsec)
	{
		if ((diff - WARN_NSEC_TOLLERENCE) > clk->interval_nsec)
		{
			LOGK(LOG_WARN, "Trigger %s has become unsynchronized (clock overshoot of %" PRIu64 " nanoseconds)", kobj_objectname(kobj_cast(trigger_cast(clk))), (diff - clk->interval_nsec));
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
	trigger_clock_t * clk = trigger_new(str.string, trigger_descclock, NULL, trigger_waitclock, sizeof(trigger_clock_t));
	clk->interval_nsec = hz2nanos(freq_hz);
	clk->freq_hz = freq_hz;

	return clk;
}

// ----------------------- VARCLOCK ------------------------
static void trigger_destroyvarclock(kobject_t * object)
{
	trigger_varclock_t * vclk = (trigger_varclock_t *)object;
	link_destroy(&vclk->links);
	port_destroy(&vclk->ports);
}

static bool trigger_waitvarclock(trigger_t * trigger)
{
	trigger_varclock_t * vclk = (void *)trigger;
	trigger_clock_t * clk = &vclk->clock;

	// Handle the input/output links
	link_doinputs(&vclk->ports, &vclk->links);
	link_dooutputs(&vclk->ports, &vclk->links);

	// Get the (possibly) new block input rate
	port_t * rateport = port_lookup(&vclk->ports, meta_input, VARCLOCK_RATE_PORT);
	iobacking_t * ratebacking = port_iobacking(rateport);

	const double * new_freq_hz = NULL;
	if (!iobacking_isnull(ratebacking))
	{
		new_freq_hz = iobacking_data(ratebacking);
	}


	uint64_t new_interval = (new_freq_hz == NULL)? clk->interval_nsec : hz2nanos(*new_freq_hz);
	if (new_interval != clk->interval_nsec)
	{
		clk->interval_nsec = new_interval;
		clk->freq_hz = *new_freq_hz;

		struct timespec now, trigger;
		clock_gettime(CLOCK_REALTIME, &now);
		memcpy(&trigger, &clk->last_trigger, sizeof(struct timespec));

		uint64_t diff = diffnanos(&now, &trigger);
		if (diff > clk->interval_nsec)
		{
			memcpy(&clk->last_trigger, &now, sizeof(struct timespec));
			return true;
		}
	}

	return trigger_waitclock(trigger);
}

trigger_varclock_t * trigger_newvarclock(const char * name, double initial_freq_hz, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(name == NULL || initial_freq_hz <= 0.0)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}

	// Create the trigger
	trigger_varclock_t * vclk = trigger_new(name, trigger_descclock, trigger_destroyvarclock, trigger_waitvarclock, sizeof(trigger_varclock_t));
	vclk->clock.interval_nsec = hz2nanos(initial_freq_hz);
	vclk->clock.freq_hz = initial_freq_hz;
	linklist_init(&vclk->links);
	portlist_init(&vclk->ports);

	// Create the rate port
	iobacking_t * ratebacking = iobacking_new('d', err);
	if (ratebacking == NULL || exception_check(err))
	{
		return NULL;
	}

	bool success = port_add(&vclk->ports, meta_input, VARCLOCK_RATE_PORT, ratebacking, err);
	if (!success || exception_check(err))
	{
		return NULL;
	}

	return vclk;
}
