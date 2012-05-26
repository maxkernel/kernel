#include <math.h>

#include <aul/common.h>

#include <map.h>
#include <kernel.h>

// Default calibration settings
#define MOTOR_CALIBRATION_TIMEOUT	(2 * MICROS_PER_SECOND)
int motor_min	= 1200,		motor_center	= 1400,		motor_max		= 1600;
int front_min	= 1200,		front_center	= 1500,		front_max		= 1800;
int rear_min	= 1200,		rear_center		= 1500,		rear_max		= 1800;


static uint64_t last_motor_update = 0;
static int pwm_motor_front = 0, pwm_motor_rear = 0, pwm_front = 0, pwm_rear = 0;
static map_t * map_motor_front = NULL, * map_motor_rear = NULL, * map_front = NULL, * map_rear = NULL;

static void setmaps()
{
	{ // Motor map
		if (map_motor_front != NULL) map_destroy(map_motor_front);
		double from[] = {-1.0, 0.0, 1.0};
		double to[] = {motor_min, motor_center, motor_max};
		map_motor_front = map_newfromarray(MAP_LINEAR, from, to, 3);
		if (map_motor_rear != NULL) map_destroy(map_motor_rear);
		map_motor_rear = map_reverse(map_motor_front);
	}

	{ // Front map
		if (map_front != NULL) map_destroy(map_front);
		double from[] = {-5.0*M_PI/36.0, 0.0, 5.0*M_PI/36.0};
		double to[] = {front_min, front_center, front_max};
		map_front = map_newfromarray(MAP_LINEAR, from, to, 3);
	}

	{ // Rear map
		if (map_rear != NULL) map_destroy(map_rear);
		double from[] = {-5.0*M_PI/36.0, 0.0, 5.0*M_PI/36.0};
		double to[] = {rear_min, rear_center, rear_max};
		map_rear = map_newfromarray(MAP_LINEAR, from, to, 3);
	}
}

static void model_calpreview(void * object, const char * domain, const char * name, const char sig, void * backing, char * hint, size_t hint_length)
{
	if (strprefix(name, "motor"))
	{
		pwm_motor_front	= *(int *)backing;
		pwm_motor_rear = motor_center;
		last_motor_update = kernel_timestamp();
	}
	else if	(strprefix(name, "front"))		pwm_front	= *(int *)backing;
	else if	(strprefix(name, "rear"))		pwm_rear	= *(int *)backing;
}

static void model_calmodechange(void * object, calmode_t mode, calstatus_t status)
{
	if (mode == calmode_runtime)
	{
		// Just completed calibration, reset the maps
		setmaps();
	}
}

void model_update(void * obj)
{
	if (cal_getmode() == calmode_runtime)
	{
		// Not in calibration mode, read inputs and apply maps

		const double * value_throttle = INPUT(throttle);
		const double * value_front = INPUT(front);
		const double * value_rear = INPUT(rear);

		if (value_throttle != NULL)
		{
			pwm_motor_front = (int)map_tovalue(map_motor_front, clamp(*value_throttle, -1.0, 1.0));
			pwm_motor_rear = (int)map_tovalue(map_motor_rear, clamp(*value_throttle, -1.0, 1.0));
		}

		if (value_front != NULL)
			pwm_front = (int)map_tovalue(map_front, clamp(*value_front, -5.0*M_PI/36.0, 5.0*M_PI/36.0));

		if (value_rear != NULL)
			pwm_rear = (int)map_tovalue(map_rear, clamp(*value_rear, -5.0*M_PI/36.0, 5.0*M_PI/36.0));
	}
	else if (kernel_timestamp() > (last_motor_update + MOTOR_CALIBRATION_TIMEOUT))
	{
		pwm_motor_front = pwm_motor_rear = motor_center;
	}

	OUTPUT(motor_front_pwm, &pwm_motor_front);
	OUTPUT(motor_rear_pwm, &pwm_motor_rear);
	OUTPUT(front_pwm, &pwm_front);
	OUTPUT(rear_pwm, &pwm_rear);
}

static bool model_init()
{
	cal_register(	NULL,	motor_min,		'i',		"[0:25:15000] Motor Minimum (full reverse). Calibrate front motor only.",	model_calpreview,	NULL);
	cal_register(	NULL,	motor_center,	'i',		"[0:25:15000] Motor Center (neutral). Calibrate front motor only.",			model_calpreview,	NULL);
	cal_register(	NULL,	motor_max,		'i',		"[0:25:15000] Motor Maximum (full forward). Calibrate front motor only.",	model_calpreview,	NULL);
	cal_register(	NULL,	front_min,		'i',		"[0:25:15000] Front Wheel Minimum (left -25 deg)",							model_calpreview,	NULL);
	cal_register(	NULL,	front_center,	'i',		"[0:25:15000] Front Wheel Centered",										model_calpreview,	NULL);
	cal_register(	NULL,	front_max,		'i',		"[0:25:15000] Front Wheel Maximum (right +25 deg)",							model_calpreview,	NULL);
	cal_register(	NULL,	rear_min,		'i',		"[0:25:15000] Rear Wheel Minimum (right +25 deg)",							model_calpreview,	NULL);
	cal_register(	NULL,	rear_center,	'i',		"[0:25:15000] Rear Wheel Centered",											model_calpreview,	NULL);
	cal_register(	NULL,	rear_max,		'i',		"[0:25:15000] Rear Wheel Maximum (left -25 deg)",							model_calpreview,	NULL);
	cal_onmodechange(model_calmodechange,	NULL);

	pwm_motor_front = pwm_motor_rear = motor_center;
	pwm_front = front_center;
	pwm_rear = rear_center;

	setmaps();
	return true;
}

static void model_destroy()
{
	map_destroy(map_motor_front);
	map_destroy(map_motor_rear);
	map_destroy(map_front);
	map_destroy(map_rear);
}


module_name("Drive Model");
module_version(1,0,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Helper module for use in applications where the robot has four wheeled steering.");
module_dependency("map");
module_oninitialize(model_init);
module_ondestroy(model_destroy);

define_block(	static,		"Static instance of the drive model", NULL, "", "");
block_input(	static, 	throttle, 			'd', 	"Energizes the motor. Value must be between -1.0 to 1.0");
block_input(	static, 	front, 				'd', 	"Turns the front wheel. Value must be radians between -5pi/36 to 5pi/36 (-/+ 25 degrees)");
block_input(	static, 	rear, 				'd', 	"Turns the rear wheel. Value must be radians between -5pi/36 to 5pi/36 (-/+ 25 degrees)");
block_output(	static, 	motor_front_pwm, 	'i', 	"The output pwm for the front motor (pwm measured in microseconds)");
block_output(	static, 	motor_rear_pwm, 	'i', 	"The output pwm for the rear motor (pwm measured in microseconds)");
block_output(	static, 	front_pwm, 			'i', 	"The output pwm for the front stearing servo (pwm measured in microseconds)");
block_output(	static, 	rear_pwm, 			'i', 	"The output pwm for the rear stearing servo (pwm measured in microseconds)");
block_onupdate(	static, 	model_update);
