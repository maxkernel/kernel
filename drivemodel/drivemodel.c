#include <math.h>

#include <aul/common.h>

#include <map.h>
#include <kernel.h>

MOD_VERSION("1.0");
MOD_AUTHOR("Andrew Klofas - <andrew@maxkernel.com>");
MOD_DESCRIPTION("Helper module for use in applications where the robot has four wheeled steering.");
MOD_DEPENDENCY("map");
MOD_INIT(model_init);
MOD_DESTROY(model_destroy);

BLK_INPUT(STATIC, throttle, "d", "Energizes the motor. Value must be between -1.0 to 1.0");
BLK_INPUT(STATIC, front, "d", "Turns the front wheel. Value must be radians between -5pi/36 to 5pi/36 (-/+ 25 degrees)");
BLK_INPUT(STATIC, rear, "d", "Turns the rear wheel. Value must be radians between -5pi/36 to 5pi/36 (-/+ 25 degrees)");
BLK_OUTPUT(STATIC, motor_front_pwm, "i");
BLK_OUTPUT(STATIC, motor_rear_pwm, "i");
BLK_OUTPUT(STATIC, front_pwm, "i");
BLK_OUTPUT(STATIC, rear_pwm, "i");
BLK_ONUPDATE(STATIC, model_update);


// Default calibration settings
#define MOTOR_CALIBRATION_TIMEOUT	(2 * MICROS_PER_SECOND)
int motor_min	= 5500,		motor_center	= 6750,		motor_max		= 8000;
int front_min	= 9900,		front_center	= 7200,		front_max		= 4500;
int rear_min	= 4500,		rear_center		= 7200,		rear_max		= 9900;

CAL_UPDATE(model_updatecal);
CAL_PREVIEW(model_previewcal);
CAL_PARAM(	motor_min,		"i0:15000",		"Motor Minimum (full reverse). Calibrate front motor only."	);
CAL_PARAM(	motor_center,	"i0:15000",		"Motor Center (neutral). Calibrate front motor only."		);
CAL_PARAM(	motor_max,		"i0:15000",		"Motor Maximum (full forward). Calibrate front motor only."	);
CAL_PARAM(	front_min,		"i0:15000",		"Front Wheel Minimum (left -25 deg)"	);
CAL_PARAM(	front_center,	"i0:15000",		"Front Wheel Centered"					);
CAL_PARAM(	front_max,		"i0:15000",		"Front Wheel Maximum (right +25 deg)"	);
CAL_PARAM(	rear_min,		"i0:15000",		"Rear Wheel Minimum (right +25 deg)"	);
CAL_PARAM(	rear_center,	"i0:15000",		"Rear Wheel Centered"					);
CAL_PARAM(	rear_max,		"i0:15000",		"Rear Wheel Maximum (left -25 deg)"		);


static bool mode_calibration = false;
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

void model_updatecal(const char * name, const char type, void * newvalue, void * target)
{
	if (type != T_INTEGER)
	{
		LOG(LOG_WARN, "Unknown calibration entry: %s", name);
		return;
	}

	memcpy(target, newvalue, sizeof(int));
	setmaps();

	mode_calibration = false;
}

void model_previewcal(const char * name, const char type, void * newvalue, void * target)
{
	mode_calibration = true;

	if (strprefix(name, "motor"))
	{
		pwm_motor_front	= *(int *)newvalue;
		pwm_motor_rear = motor_center;
		last_motor_update = kernel_timestamp();
	}
	else if	(strprefix(name, "front"))		pwm_front	= *(int *)newvalue;
	else if	(strprefix(name, "rear"))		pwm_rear	= *(int *)newvalue;
}

void model_update(void * obj)
{
	if (mode_calibration == false)
	{
		const double * value_throttle = INPUT(throttle);
		const double * value_front = INPUT(front);
		const double * value_rear = INPUT(rear);

		if (value_throttle != NULL)
		{
			pwm_motor_front = (int)map_tovalue(map_motor_front, CLAMP(*value_throttle, -1.0, 1.0));
			pwm_motor_rear = (int)map_tovalue(map_motor_rear, CLAMP(*value_throttle, -1.0, 1.0));
		}

		if (value_front != NULL)
			pwm_front = (int)map_tovalue(map_front, CLAMP(*value_front, -5.0*M_PI/36.0, 5.0*M_PI/36.0));

		if (value_rear != NULL)
			pwm_rear = (int)map_tovalue(map_rear, CLAMP(*value_rear, -5.0*M_PI/36.0, 5.0*M_PI/36.0));
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

void model_init()
{
	pwm_motor_front = pwm_motor_rear = motor_center;
	pwm_front = front_center;
	pwm_rear = rear_center;

	setmaps();
}

void model_destroy()
{
	map_destroy(map_motor_front);
	map_destroy(map_motor_rear);
	map_destroy(map_front);
	map_destroy(map_rear);
}
