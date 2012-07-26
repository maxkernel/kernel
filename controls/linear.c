#include <kernel.h>


typedef struct
{
	double m, b;
} linear_t;

static void * linear_new(double m, double b)
{
	linear_t * l = malloc(sizeof(linear_t));
	memset(l, 0, sizeof(linear_t));
	l->m = m;
	l->b = b;

	return l;
}

static void linear_update(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	const double * value = input(value);
	if (value == NULL)
	{
		return;
	}

	linear_t * l = object;
	double v = l->m * *value + l->b;
	output(value, &v);
}

static void linear_destroy(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	linear_t * l = object;
	free(l);
}


define_block(linear, "Linear equation y=mx+b", linear_new, "dd", "");
block_onupdate(linear, linear_update);
block_ondestroy(linear, linear_destroy);
block_input(linear, value, T_DOUBLE, "Input value");
block_output(linear, value, T_DOUBLE, "Output value");
