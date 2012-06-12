#include <math.h>

#include <aul/common.h>

#include <map.h>
#include <kernel.h>

// Default calibration settings
#define MOTOR_CALIBRATION_TIMEOUT	(2 * MICROS_PER_SECOND)


typedef struct
{
	struct
	{	// TODO - remove one of these to make it linear?
		int motor_min, motor_center, motor_max;
		int front_min, front_center, front_max;
		int rear_min, rear_center, rear_max;
	} cal;
	
	struct
	{
		int motor_front, motor_rear, front, rear;
	} pwm;

	struct
	{
		map_t * motor_front, * motor_rear, * front, * rear;
	} map;

	uint64_t last_motor_update;
} quadrasteer_t;

static void setmaps(quadrasteer_t * q)
{
	// Motor map
	{
		if (q->map.motor_front != NULL) map_destroy(q->map.motor_front);
		double from[] = {-1.0, 0.0, 1.0};
		double to[] = {q->cal.motor_min, q->cal.motor_center, q->cal.motor_max};
		q->map.motor_front = map_newfromarray(MAP_LINEAR, from, to, 3);
		
		if (q->map.motor_rear != NULL) map_destroy(q->map.motor_rear);
		q->map.motor_rear = map_reverse(q->map.motor_front);
	}

	// Front map
	{
		if (q->map.front != NULL) map_destroy(q->map.front);
		double from[] = {-5.0*M_PI/36.0, 0.0, 5.0*M_PI/36.0};
		double to[] = {q->cal.front_min, q->cal.front_center, q->cal.front_max};
		q->map.front = map_newfromarray(MAP_LINEAR, from, to, 3);
	}

	// Rear map
	{
		if (q->map.rear != NULL) map_destroy(q->map.rear);
		double from[] = {-5.0*M_PI/36.0, 0.0, 5.0*M_PI/36.0};
		double to[] = {q->cal.rear_min, q->cal.rear_center, q->cal.rear_max};
		q->map.rear = map_newfromarray(MAP_LINEAR, from, to, 3);
	}
}

static void calpreview(void * object, const char * domain, const char * name, const char sig, void * backing, char * hint, size_t hint_length)
{
	quadrasteer_t * q = object;

	if (strprefix(name, "motor"))
	{
		q->pwm.motor_front	= *(int *)backing;
		q->pwm.motor_rear = q->cal.motor_center;
		q->last_motor_update = kernel_timestamp();
	}
	else if	(strprefix(name, "front"))		q->pwm.front	= *(int *)backing;
	else if	(strprefix(name, "rear"))		q->pwm.rear		= *(int *)backing;
}

static void calmodechange(void * object, calmode_t mode, calstatus_t status)
{
	quadrasteer_t * q = object;

	if (mode == calmode_runtime)
	{
		// Just completed calibration, reset the maps
		setmaps(q);
	}
}

static void quadrasteer_update(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	quadrasteer_t * q = object;

	if (cal_getmode() == calmode_runtime)
	{
		// Not in calibration mode, read inputs and apply maps

		const double * value_throttle = INPUT(throttle);
		const double * value_front = INPUT(front);
		const double * value_rear = INPUT(rear);

		if (value_throttle != NULL)
		{
			q->pwm.motor_front = (int)map_tovalue(q->map.motor_front, clamp(*value_throttle, -1.0, 1.0));
			q->pwm.motor_rear = (int)map_tovalue(q->map.motor_rear, clamp(*value_throttle, -1.0, 1.0));
		}

		if (value_front != NULL)
			q->pwm.front = (int)map_tovalue(q->map.front, clamp(*value_front, -5.0*M_PI/36.0, 5.0*M_PI/36.0));

		if (value_rear != NULL)
			q->pwm.rear = (int)map_tovalue(q->map.rear, clamp(*value_rear, -5.0*M_PI/36.0, 5.0*M_PI/36.0));
	}
	else if (kernel_timestamp() > (q->last_motor_update + MOTOR_CALIBRATION_TIMEOUT))
	{
		q->pwm.motor_front = q->pwm.motor_rear = q->cal.motor_center;
	}

	OUTPUT(motor_front_pwm, &q->pwm.motor_front);
	OUTPUT(motor_rear_pwm, &q->pwm.motor_rear);
	OUTPUT(front_pwm, &q->pwm.front);
	OUTPUT(rear_pwm, &q->pwm.rear);
}

static void * quadrasteer_new()
{
	quadrasteer_t * q = malloc(sizeof(quadrasteer_t));
	memset(q, 0, sizeof(quadrasteer_t));
	
	// Set the default calibration settings
	q->cal.motor_min	= 1200;		q->cal.motor_center		= 1400;		q->cal.motor_max	= 1600;
	q->cal.front_min	= 1200;		q->cal.front_center		= 1500;		q->cal.front_max	= 1800;
	q->cal.rear_min		= 1200;		q->cal.rear_center		= 1500;		q->cal.rear_max		= 1800;

	// Register with the calibration subsystem
	cal_register(	NULL,	"motor_min",	&q->cal.motor_min,		'i',		"[0:25:15000] Motor Minimum (full reverse). Calibrate front motor only.",	calpreview,	q);
	cal_register(	NULL,	"motor_center",	&q->cal.motor_center,	'i',		"[0:25:15000] Motor Center (neutral). Calibrate front motor only.",			calpreview,	q);
	cal_register(	NULL,	"motor_max",	&q->cal.motor_max,		'i',		"[0:25:15000] Motor Maximum (full forward). Calibrate front motor only.",	calpreview,	q);
	cal_register(	NULL,	"front_min",	&q->cal.front_min,		'i',		"[0:25:15000] Front Wheel Minimum (left -25 deg)",							calpreview,	q);
	cal_register(	NULL,	"front_center",	&q->cal.front_center,	'i',		"[0:25:15000] Front Wheel Centered",										calpreview,	q);
	cal_register(	NULL,	"front_max",	&q->cal.front_max,		'i',		"[0:25:15000] Front Wheel Maximum (right +25 deg)",							calpreview,	q);
	cal_register(	NULL,	"rear_min",		&q->cal.rear_min,		'i',		"[0:25:15000] Rear Wheel Minimum (right +25 deg)",							calpreview,	q);
	cal_register(	NULL,	"rear_center",	&q->cal.rear_center,	'i',		"[0:25:15000] Rear Wheel Centered",											calpreview,	q);
	cal_register(	NULL,	"rear_max",		&q->cal.rear_max,		'i',		"[0:25:15000] Rear Wheel Maximum (left -25 deg)",							calpreview,	q);
	cal_onmodechange(calmodechange, q);

	// Set the initial pwm outputs
	q->pwm.motor_front = q->pwm.motor_rear = q->cal.motor_center;
	q->pwm.front = q->cal.front_center;
	q->pwm.rear = q->cal.rear_center;

	// Initialize the calibration mappings
	setmaps(q);

	return q;
}

static void quadrasteer_destroy(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	quadrasteer_t * q = object;
	map_destroy(q->map.motor_front);
	map_destroy(q->map.motor_rear);
	map_destroy(q->map.front);
	map_destroy(q->map.rear);
}


module_dependency("map");

define_block(	quadrasteer,	"Quadrasteer rover model. Includes caibration", quadrasteer_new, "", "No constructor parameters");
block_onupdate(	quadrasteer, 	quadrasteer_update);
block_ondestroy(quadrasteer,	quadrasteer_destroy);
block_input(	quadrasteer, 	throttle, 			'd', 	"Energizes the motor. Value must be between -1.0 to 1.0");
block_input(	quadrasteer, 	front, 				'd', 	"Turns the front wheel. Value must be radians between -5pi/36 to 5pi/36 (-/+ 25 degrees)");
block_input(	quadrasteer, 	rear, 				'd', 	"Turns the rear wheel. Value must be radians between -5pi/36 to 5pi/36 (-/+ 25 degrees)");
block_output(	quadrasteer, 	motor_front_pwm, 	'i', 	"The output pwm for the front motor (pwm measured in microseconds)");
block_output(	quadrasteer, 	motor_rear_pwm, 	'i', 	"The output pwm for the rear motor (pwm measured in microseconds)");
block_output(	quadrasteer, 	front_pwm, 			'i', 	"The output pwm for the front stearing servo (pwm measured in microseconds)");
block_output(	quadrasteer, 	rear_pwm, 			'i', 	"The output pwm for the rear stearing servo (pwm measured in microseconds)");
