#include <kernel.h>


static void * pi_new()
{
	return NULL;
}

static void pi_update(void * object)
{

}

static void pi_destroy(void * object)
{

}


define_block(pi, "PI control loop", pi_new, "", "");
block_onupdate(pi, pi_update);
block_ondestroy(pi, pi_destroy);
//block_input(unit, value, T_DOUBLE, "Input value (-1.0 to 1.0)");
//block_output(unit, value, T_DOUBLE, "Output value (determined by calibration)");

