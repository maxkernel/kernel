#include <string.h>
#include <math.h>
#include <inttypes.h>

#include <kernel.h>
#include <serial.h>
#include <map.h>

MOD_VERSION("0.9");
MOD_AUTHOR("Andrew Klofas <andrew.klofas@senseta.com>");
MOD_DESCRIPTION("Talks to the Pololu Micro Serial Servo Controller");
MOD_DEPENDENCY("serial", "map");
MOD_INIT(module_init);


int motor_min	= 0,		motor_center	= 100,		motor_max			= 200;
int front_min	= 0,		front_center		= 100,		front_max				= 200;
int rear_min		= 0,		rear_center		= 100,		rear_max				= 200;
int pan_min		= 0,		pan_forward		= 50,		pan_backward		= 150,			pan_max = 200;
int tilt_min		= 0,		tilt_center			= 100,		tilt_max				= 200;

static map_t * map_motor	= NULL;
static map_t * map_front		= NULL;
static map_t * map_rear		= NULL;
static map_t * map_pan		= NULL;
static map_t * map_tilt			= NULL;

//calibration values
CAL_UPDATE(mssc_updatecal);
CAL_PARAM(	motor_min,		"i0:255",		"Motor Minimum (full reverse)"					);
CAL_PARAM(	motor_center,	"i0:255",		"Motor Center (neutral)"							);
CAL_PARAM(	motor_max,		"i0:255",		"Motor Maximum (full forward)"					);
CAL_PARAM(	front_min,			"i0:255",		"Front Wheel Minimum (left -25 deg)"		);
CAL_PARAM(	front_center,		"i0:255",		"Front Wheel Center"								);
CAL_PARAM(	front_max,		"i0:255",		"Front Wheel Maximum (right +25 deg)"	);
CAL_PARAM(	rear_min,			"i0:255",		"Rear Wheel Minimum (right +25 deg)"		);
CAL_PARAM(	rear_center,		"i0:255",		"Rear Wheel Center"									);
CAL_PARAM(	rear_max,			"i0:255",		"Rear Wheel Maximum (left -25 deg)"		);
CAL_PARAM(	pan_min,			"i0:255",		"Head Pan Minimum (left -270 deg)"			);
CAL_PARAM(	pan_backward,	"i0:255",		"Head Pan Backward Center"						);
CAL_PARAM(	pan_forward,		"i0:255",		"Head Pan Forward Center"						);
CAL_PARAM(	pan_max,			"i0:255",		"Head Pan Maximum (right +90 deg)"		);
CAL_PARAM(	tilt_min,			"i0:255",		"Head Tilt Minimum (down -45 deg)"			);
CAL_PARAM(	tilt_center,		"i0:255",		"Head Tilt Center (level forward)"				);
CAL_PARAM(	tilt_max,			"i0:255",		"Head Tilt Maximum (up +90 deg)"			);


char * serial_port					= "/dev/ttyS0";
int motor_front_channel		= 0;
int motor_rear_channel			= 1;
int front_channel					= 2;
int rear_channel					= 3;
int pan_channel					= 4;
int tilt_channel						= 5;

CFG_PARAM(	serial_port,					"s");
CFG_PARAM(	motor_front_channel,	"i");
CFG_PARAM(	motor_rear_channel,		"i");
CFG_PARAM(	front_channel,				"i");
CFG_PARAM(	rear_channel,				"i");
CFG_PARAM(	pan_channel,				"i");
CFG_PARAM(	tilt_channel,					"i");


DEF_SYSCALL(	motor,		"v:d",		"Energizes the motor by given value (param 1) between -1.0 to 1.0");
DEF_SYSCALL(	motoroff,	"v:v",		"Turns off motor (Note: does not brake)");
DEF_SYSCALL(	alloff,		"v:v",		"Turns off all actuators");
DEF_SYSCALL(	turn,			"v:d",		"Turns front and rear wheels by given value (param 1) radians between -5pi/36 to 5pi/36 (-/+ 25 degrees). Also applies anti-roll algoritm if enabled");
DEF_SYSCALL(	turnfront,	"v:d",		"Turns the front wheel by given value (param 1) radians between -5pi/36 to 5pi/36 (-/+ 25 degrees)");
DEF_SYSCALL(	turnrear,	"v:d",		"Turns the rear wheel by given value (param 1) radians between -5pi/36 to 5pi/36 (-/+ 25 degrees)");
DEF_SYSCALL(	pan,			"v:d",		"Rotates the head by given value (param 1) in radians. 0 rad is center forward, positive moves clockwise.");
DEF_SYSCALL(	tilt,			"v:d",		"Tilts the head by given value (param 1) radians between -pi/4 ... pi/2 (-45 degrees to +90 degrees)");


#define HEADER			0xFF

static int mssc_fd = -1;
static double motor_throttle = 0.0;

static void setmaps();

static inline void mssc_write(uint8_t servonum, uint8_t pos)
{
	if (mssc_fd == -1)
	{
		LOG(LOG_ERR, "Attempting to access MSSC when not connected!");
		return;
	}

	char data[3] = {HEADER, servonum, pos};
	write(mssc_fd, data, sizeof(data));
}


/* ------------------- SYSCALLS ----------------------- */
void motor(double value)
{
	motor_throttle = CLAMP(value, -1.0, 1.0);

	//pod_write_int(PCMD_PWM0 + motor_front_channel, (gint)cal_map(motor_min, motor_center, motor_max, -1.0, 0.0, 1.0, motor_throttle * direction));
	//pod_write_int(PCMD_PWM0 + motor_rear_channel, (gint)cal_map(motor_max, motor_center, motor_min, -1.0, 0.0, 1.0, motor_throttle * direction));
}

void motoroff()
{
	//mssc_write(motor_front_channel, (int)map_tovalue(map_motor, 0.0));
	//mssc_write(motor_rear_channel, (int)map_tovalue(map_motor, 0.0));
}

void alloff()
{
	motoroff();
	/*
	mssc_write(CMD_PARAMS, front_channel, 0, 0, 1);
	mssc_write(CMD_PARAMS, rear_channel, 0, 0, 1);
	mssc_write(CMD_PARAMS, pan_channel, 0, 0, 1);
	mssc_write(CMD_PARAMS, tilt_channel, 0, 0, 1);
	*/
}

void turnfront(double value)
{
	mssc_write(front_channel, (int)map_tovalue(map_front, CLAMP(value, -5.0*M_PI/36.0, 5.0*M_PI/36.0)));

	//pod_write_int(PCMD_PWM0 + front_channel, (gint)cal_map(front_min, front_center, front_max, -5.0*M_PI/36.0, 0.0, 5.0*M_PI/36.0, value));
}

void turnrear(double value)
{
	mssc_write(rear_channel, (int)map_tovalue(map_rear, CLAMP(value, -5.0*M_PI/36.0, 5.0*M_PI/36.0)));

	//pod_write_int(PCMD_PWM0 + rear_channel, (gint)cal_map(rear_min, rear_center, rear_max, -5.0*M_PI/36.0, 0.0, 5.0*M_PI/36.0, value));
}

void turn(double value)
{
	double front = CLAMP(value, -5.0*M_PI/36.0, 5.0*M_PI/36.0);
	double rear = CLAMP(value, -5.0*M_PI/36.0, 5.0*M_PI/36.0);

	/*
	if (enable_antiroll && motor_throttle >= 0.0)
	{
		//limit the front turn range (curve looks like:
		// 0 - 0.5 throttle = turn unaffected
		// 1 throttle = turn down by 2/3
		// with linear interpolation
		if (motor_throttle >= 0.5)
		{
			front *= -4.0/3.0*motor_throttle+5.0/3.0;
		}

		//limit the rear turn range
		rear *= 1.0-motor_throttle;
	}
	*/

	turnfront(front);
	turnrear(rear);
}

void pan(double value)
{
	LOG(LOG_WARN, "TODO: Pan implementation unfinished");
}

void tilt(double value)
{
	LOG(LOG_WARN, "TODO: Tilt implementation unfinished");
}

void mssc_updatecal(const char * name, const char type, void * newvalue, void * target, const int justpreview)
{
	if (type != T_INTEGER)
	{
		LOG(LOG_WARN, "Unknown calibration entry: %s", name);
		return;
	}

	if (justpreview)
	{
		if (g_str_has_prefix(name, "motor"))
		{
			LOG(LOG_INFO, "UPDATECAL MOTOR TODO");
		}
		else if (g_str_has_prefix(name, "front")) { mssc_write(front_channel, *(int *)newvalue); }
		else if (g_str_has_prefix(name, "rear")) { mssc_write(rear_channel, *(int *)newvalue); }
		//else if (g_str_has_prefix(name, "pan")) { LOG(LOG_INFO, "UPDATECAL PAN TODO"); }
		//else if (g_str_has_prefix(name, "tilt")) { LOG(LOG_INFO, "UPDATECAL TILT TODO"); }
	}
	else
	{
		memcpy(target, newvalue, sizeof(int));
		setmaps();
	}
}


void module_init() {
	mssc_fd = serial_open_bin(serial_port, B9600, NULL);
	setmaps();
}

static void setmaps()
{
	//motor
	if (map_motor != NULL) map_destroy(map_motor);
	double motor_from[] = {-1.0, 0.0, 1.0};
	double motor_to[] = {motor_min, motor_center, motor_max};
	map_motor = map_newfromarray(MAP_LINEAR, motor_from, motor_to, 3);

	//front
	if (map_front != NULL) map_destroy(map_front);
	double front_from[] = {-5.0*M_PI/36.0, 0.0, 5.0*M_PI/36.0};
	double front_to[] = {front_min, front_center, front_max};
	map_front = map_newfromarray(MAP_LINEAR, front_from, front_to, 3);

	//rear
	if (map_rear != NULL) map_destroy(map_rear);
	double rear_from[] = {-5.0*M_PI/36.0, 0.0, 5.0*M_PI/36.0};
	double rear_to[] = {rear_min, rear_center, rear_max};
	map_rear = map_newfromarray(MAP_LINEAR, rear_from, rear_to, 3);

	//pan
	if (map_pan != NULL) map_destroy(map_pan);
	double pan_from[] = {-M_PI, 0.0, M_PI/2.0, M_PI/2.0+0.0001, M_PI};
	double pan_to[] = {pan_backward, pan_forward, pan_max, pan_min, pan_backward};
	map_pan = map_newfromarray(MAP_LINEAR, pan_from, pan_to, 5);

	//tilt
	if (map_tilt != NULL) map_destroy(map_tilt);
	double tilt_from[] = {-M_PI/4.0, 0.0, M_PI/2.0};
	double tilt_to[] = {tilt_min, tilt_center, tilt_max};
	map_tilt = map_newfromarray(MAP_LINEAR, tilt_from, tilt_to, 3);
}
