#include <kernel.h>


typedef struct
{
	double p, d;
	double last_error;
} pd_t;

static void * pd_new(double p, double d)
{
	pd_t * pd = malloc(sizeof(pd_t));
	memset(pd, 0, sizeof(pd_t));
	pd->p = p;
	pd->d = d;
	pd->last_error = 0;

	return pd;
}

static void pd_update(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	const double * setpoint = input(setpoint);
	const double * feedback = input(feedback);

	if (setpoint == NULL || feedback == NULL)
	{
		return;
	}

	pd_t * pd = object;
	double error = *setpoint - *feedback;

	double value = (error * pd->p) + ((error - pd->last_error) * pd->d);
	output(value, &value);

	pd->last_error = error;
}

static void pd_destroy(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	pd_t * pd = object;
	free(pd);
}


define_block(pd, "PD control loop", pd_new, "dd", "");
block_onupdate(pd, pd_update);
block_ondestroy(pd, pd_destroy);
block_input(pd, setpoint, T_DOUBLE, "The setpoint input");
block_input(pd, feedback, T_DOUBLE, "The feedback input");
block_output(pd, value, T_DOUBLE, "Output value");
