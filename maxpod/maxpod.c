#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <unistd.h>

// TODO - delete this and remove GThread
#include <glib.h>

#include <kernel.h>
#include <map.h>
#include <array.h>
#include <aul/serial.h>
#include <aul/queue.h>
#include <aul/mainloop.h>

MOD_VERSION("1.0");
MOD_AUTHOR("Andrew Klofas - andrew.klofas@senseta.com");
MOD_DESCRIPTION("Driver for MaxPOD (Forth) IO Board.");
MOD_DEPENDENCY("map");
MOD_INIT(mod_init);
MOD_DESTROY(mod_destroy);

//config and calibration settings and default values
int motor_min	= 5500,		motor_center	= 6750,		motor_max		= 8000;
int front_min	= 9900,		front_center	= 7200,		front_max		= 4500;
int rear_min	= 4500,		rear_center		= 7200,		rear_max		= 9900;
int pan_min		= 5000,		pan_forward		= 8000,		pan_backward	= 6000,			pan_max		= 9000;
int tilt_min	= 5000,		tilt_center		= 7000,		tilt_max		= 9000;

char * serial_port				= "/dev/ttyS0";
int motor_front_channel			= 0;
int motor_rear_channel			= 1;
int front_channel				= 2;
int rear_channel				= 3;
int pan_channel					= 4;
int tilt_channel				= 5;

bool enable_antiroll = true;
bool enable_slowreverse = true;
bool enable_directiontoggle = true;

//static functions
static void setmaps();

//global functions
void motoroff();

//calibration values
CAL_UPDATE(pod_updatecal);
CAL_PARAM(	motor_min,		"i0:15000",		"Motor Minimum (full reverse)"			);
CAL_PARAM(	motor_center,	"i0:15000",		"Motor Center (neutral)"				);
CAL_PARAM(	motor_max,		"i0:15000",		"Motor Maximum (full forward)"			);
CAL_PARAM(	front_min,		"i0:15000",		"Front Wheel Minimum (left -25 deg)"	);
CAL_PARAM(	front_center,	"i0:15000",		"Front Wheel Center"					);
CAL_PARAM(	front_max,		"i0:15000",		"Front Wheel Maximum (right +25 deg)"	);
CAL_PARAM(	rear_min,		"i0:15000",		"Rear Wheel Minimum (right +25 deg)"	);
CAL_PARAM(	rear_center,	"i0:15000",		"Rear Wheel Center"						);
CAL_PARAM(	rear_max,		"i0:15000",		"Rear Wheel Maximum (left -25 deg)"		);
CAL_PARAM(	pan_min,		"i0:15000",		"Head Pan Minimum (left -270 deg)"		);
CAL_PARAM(	pan_forward,	"i0:15000",		"Head Pan Center Forward"				);
CAL_PARAM(	pan_backward,	"i0:15000",		"Head Pan Center Backward"				);
CAL_PARAM(	pan_max,		"i0:15000",		"Head Pan Maximum (right +90 deg)"		);
CAL_PARAM(	tilt_min,		"i0:15000",		"Head Tilt Minimum (down -45 deg)"		);
CAL_PARAM(	tilt_center,	"i0:15000",		"Head Tilt Center (level forward)"		);
CAL_PARAM(	tilt_max,		"i0:15000",		"Head Tilt Maximum (up +90 deg)"		);

//config values
CFG_PARAM(	serial_port,			"s");
CFG_PARAM(	motor_front_channel,	"i");
CFG_PARAM(	motor_rear_channel,		"i");
CFG_PARAM(	front_channel,			"i");
CFG_PARAM(	rear_channel,			"i");
CFG_PARAM(	pan_channel,			"i");
CFG_PARAM(	tilt_channel,			"i");

CFG_PARAM(	enable_antiroll,			"i",	"Enable anti-roll turning assist (forward drive only)"		);
CFG_PARAM(	enable_slowreverse,			"i",	"Enable throttle inhibitor in reverse (half speed)"			);
CFG_PARAM(	enable_directiontoggle,		"i",	"Enable symmetric driving. If toggled, controls invert"		);


#define	DLE						40
#define	STX						41
#define	ETX						126

#define PCMD_HEARTBEAT			0
#define PCMD_ECHO				1
#define PCMD_PWMBASE			2
#define PCMD_PWMOFF				8
#define PCMD_PWMALLOFF			9
#define PCMD_SETPOLL			10
#define PCMD_ERROR				11

#define PPOLL_FAILSAFE			0
#define PPOLL_HEARTBEAT			1
#define PPOLL_SONAR				2
#define PPOLL_BATTERY			3

#define PERR_UNKNOWN_CMD		0
#define PERR_MALFORMED_CMD		1
#define PERR_PARITY_FAILURE		2
#define PERR_OUT_OF_SEQUENCE	3

#define PACKET_SIZE				6

/* ------------------ POD UTILS ------------------*/
//DEF_CLOCK(pod_heartbeat, 3);	//update at 3 hertz
//DEF_CLOCK(pod_sendecho, 1);		//update at 1 hertz

#define QUEUE_SIZE					(PACKET_SIZE * 5)

#define POD_MAXLATENCY				0.5		/* seconds */
#define POD_MOTORPREVIEW_TIME		(3 * MICROS_PER_SECOND)		/* 3 seconds */

static int pod_fd = -1;
//static GTimer * pod_latency_timer = NULL;
static double motor_throttle = 0.0;
static int direction = 1;
static int64_t motorpreview_expire = 0;
static queue_t queue;
static char data[QUEUE_SIZE];

static map_t * map_motor			= NULL;
static map_t * map_motor_rev		= NULL;
static map_t * map_front			= NULL;
static map_t * map_front_rev		= NULL;
static map_t * map_rear				= NULL;
static map_t * map_rear_rev			= NULL;
static map_t * map_pan				= NULL;
static map_t * map_tilt				= NULL;

static inline void pod_write(uint8_t cmd, uint8_t param1, uint8_t param2)
{
	if (pod_fd == -1)
	{
		LOG(LOG_WARN, "Attempting to access MaxPOD when not connected!");
		return;
	}

	char data[6] = {STX, param1, param2, cmd, (param1+param2+cmd)%256, ETX};
	write(pod_fd, data, sizeof(data));
}

static inline void pod_write_pad()
{
	if (pod_fd == -1)
		return;

	char data[6];
	memset(data, DLE, sizeof(data));
	write(pod_fd, data, sizeof(data));
}

static inline void pod_write_int(uint8_t cmd, uint16_t param)
{
	pod_write(cmd, (param & 0xFF00)>>8, param & 0xFF);
}

static void pod_heartbeat(void * userdata)
{
	if (pod_fd == -1)
		return;

	pod_write(PCMD_HEARTBEAT, 0, 0);
/*
	//check to see if pod is still alive
	if (pod_alive != NULL)
	{
		if (!*pod_alive)
		{
			LOG(LOG_ERR, "MaxPOD is not responding!");

			g_free(pod_alive);
			pod_alive = NULL;
		}
		else
		{
			*pod_alive = false;
		}
	}
*/
}

static void pod_motorpreview(void * userdata)
{
	if (motorpreview_expire != 0 && kernel_timestamp() > motorpreview_expire)
	{
		motoroff();
		motorpreview_expire = 0;
	}
}

/*
static void pod_sendecho(int refresh)
{
	if (pod_fd == -1)
		return;

	if (pod_latency_timer == NULL)
	{
		pod_latency_timer = g_timer_new();
	}

	if (pod_alive != NULL && *pod_alive)
	{
		g_timer_start(pod_latency_timer);
		pod_write_int(PCMD_ECHO, 0x1234);
	}
}
*/

//static bool pod_newdata(GIOChannel * gio, GIOCondition condition, void * empty)
static bool pod_newdata(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	static bool pod_initial_response; /* false */

	char d;
	ssize_t numread;

	numread = read(fd, &d, sizeof(char));
	if (numread != sizeof(char))
	{
		//end stream!
		LOG(LOG_WARN, "End of stream reached in MaxPOD serial port %s", serial_port);
		return false;
	}
	queue_enqueue(&queue, &d, sizeof(char));

	while (true)
	{
		if (queue_size(&queue) < PACKET_SIZE)
		{
			// We don't have a full packet
			return true;
		}

		//dump all DLE's not in a packet
		queue_dequeue(&queue, &d, sizeof(char));

		if (d == STX)
		{
			break;
		}
		else if (d != DLE)
		{
			LOG(LOG_WARN, "Out of sync byte read from MaxPOD, expected DLE (%d), got %d", DLE, d);
		}
	}

	// Parse packet
	{
		int packet[PACKET_SIZE - 1];

		queue_dequeue(&queue, packet, sizeof(packet));

		if (packet[4] != ETX)
		{
			if (!pod_initial_response)
				return true;

			//frame error
			LOG(LOG_WARN, "Frame error in MaxPOD");
			pod_write(PCMD_ERROR, PERR_MALFORMED_CMD, 0);
		}

		if ( (packet[0]+packet[1]+packet[2]) % 256 != packet[3] )
		{
			//checksum bad
			LOG(LOG_WARN, "Checksum error in MaxPOD");
			pod_write(PCMD_ERROR, PERR_MALFORMED_CMD, 0);
		}

		pod_initial_response = true;

		switch (packet[2])
		{
			case PCMD_HEARTBEAT:
			{
				/*
				if (pod_alive == NULL)
					pod_alive = g_malloc(sizeof(bool));
				*pod_alive = true;
				*/

				break;
			}

			case PCMD_ECHO:
			{
				/*
				g_timer_stop(pod_latency_timer);
				if (g_timer_elapsed(pod_latency_timer, NULL) > POD_MAXLATENCY)
				{
					LOG(LOG_WARN, "MaxPOD is responding very slowely! (in %f seconds)", g_timer_elapsed(pod_latency_timer, NULL));
				}
				*/

				break;
			}

			case PCMD_ERROR:
				LOG(LOG_ERR, "Error received from MaxPOD (opcode=%d, param=%d)", packet[0], packet[1]);
				pod_write_pad();
				break;

			default:
				LOG(LOG_WARN, "Unknown command received from MaxPOD");
				break;
		}
	}

	return true;
}

/* -------- SYSCALLS ------------------*/
DEF_SYSCALL(	motor,				"v:d",		"Energizes the motor by given value (param 1) between -1.0 to 1.0");
DEF_SYSCALL(	motoroff,			"v:v",		"Turns off motor (Note: does not brake)");
DEF_SYSCALL(	alloff,				"v:v",		"Turns off all actuators");
DEF_SYSCALL(	turn,				"v:d",		"Turns front and rear wheels by given value (param 1) radians between -5pi/36 to 5pi/36 (-/+ 25 degrees). Also applies anti-roll algoritm if enabled");
DEF_SYSCALL(	turnfront,			"v:d",		"Turns the front wheel by given value (param 1) radians between -5pi/36 to 5pi/36 (-/+ 25 degrees)");
DEF_SYSCALL(	turnrear,			"v:d",		"Turns the rear wheel by given value (param 1) radians between -5pi/36 to 5pi/36 (-/+ 25 degrees)");
DEF_SYSCALL(	pan,				"v:d",		"Rotates the head by given value (param 1) radians between -3pi/2 to pi/2 (-270 degrees to +90 degrees)");
DEF_SYSCALL(	tilt,				"v:d",		"Tilts the head by given value (param 1) radians between -pi/4 ... pi/2 (-45 degrees to +90 degrees)");

//DEF_SYSCALL(	pod_latency,		"d:v",		"Returns the MaxPOD latency in seconds since last poll");
DEF_SYSCALL(	direction_set,		"i:i",		"Sets the direction (1 or -1) of the rover and returns the new direction. Note: 'enable_directiontoggle' needs to be enabled");
DEF_SYSCALL(	direction_toggle,	"i:v",		"Toggles the direction and returns the new direction (1 or -1). Note: 'enable_directiontoggle' needs to be enabled");

//BLK_INPUT(STATIC,		pan,		"d",		"Rotates the head. Accepts radians between -pi to pi (-180 degrees to +180 degrees)");
//BLK_INPUT(STATIC,		tilt,		"d",		"Tilts the head. Accepts radians between -pi/4 ... pi/2 (-45 degrees to +90 degrees)");

void motor(double value)
{
	motor_throttle = CLAMP(value, -1.0, 1.0);

	if (enable_slowreverse && motor_throttle < 0.0)
	{
		//slow down reverse by 1/4
		motor_throttle *= 0.75;
	}

	pod_write_int(PCMD_PWMBASE + motor_front_channel, (int)map_tovalue(map_motor, motor_throttle * direction));
	pod_write_int(PCMD_PWMBASE + motor_rear_channel, (int)map_tovalue(map_motor_rev, motor_throttle * direction));
}

void motoroff()
{
	//Prevent a specific motor glitch that causes the motor controllers to interpret a PWM_OFF command
	//as a brief full-forward command (Ahhhh)
	//pod_write(PCMD_PWMOFF, motor_front_channel, 0);
	//pod_write(PCMD_PWMOFF, motor_rear_channel, 0);
	motor(0.0);
}

void alloff()
{
	motoroff();
	pod_write(PCMD_PWMOFF, front_channel, 0);
	pod_write(PCMD_PWMOFF, rear_channel, 0);
	pod_write(PCMD_PWMOFF, pan_channel, 0);
	pod_write(PCMD_PWMOFF, tilt_channel, 0);
}

void turnfront(double value)
{
	//use reverse map if the direction is -1, plus invert the value
	map_t * map = direction == 1? map_front : map_front_rev;
	value *= direction;

	pod_write_int(PCMD_PWMBASE + front_channel, (int)map_tovalue(map, CLAMP(value, -5.0*M_PI/36.0, 5.0*M_PI/36.0)));
}

void turnrear(double value)
{
	map_t * map = direction == 1? map_rear : map_rear_rev;
	value *= direction;

	pod_write_int(PCMD_PWMBASE + rear_channel, (int)map_tovalue(map, CLAMP(value, -5.0*M_PI/36.0, 5.0*M_PI/36.0)));
}

void turn(double value)
{
	double front = CLAMP(value, -5.0*M_PI/36.0, 5.0*M_PI/36.0);
	double rear = CLAMP(value, -5.0*M_PI/36.0, 5.0*M_PI/36.0);

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

	turnfront(front);
	turnrear(rear);
}

void pan(double value)
{
	while (value > M_PI) { value -= 2.0*M_PI; }
	while (value < -M_PI) { value += 2.0*M_PI; }

	pod_write_int(PCMD_PWMBASE + pan_channel, (int)map_tovalue(map_pan, value));
}

void tilt(double value)
{
	pod_write_int(PCMD_PWMBASE + tilt_channel, (int)map_tovalue(map_tilt, value));
}

/*
double pod_latency()
{
	if (pod_latency_timer == NULL)
		return 0.0;

	return g_timer_elapsed(pod_latency_timer, NULL);
}
*/

int direction_set(int newdirection)
{
	if (enable_directiontoggle && (newdirection == 1 || newdirection == -1))
	{
		direction = newdirection;
	}

	return direction;
}

int direction_toggle()
{
	if (enable_directiontoggle)
	{
		direction *= -1;
	}

	return direction;
}


static GThread * motor_preview_thread = NULL;
static GTimeVal * motor_preview_killtime = NULL;
static void * pod_updatecal_motor(void * udata)
{
	motor_preview_thread = g_thread_self();

	GTimeVal now;
	GTimeVal * then = motor_preview_killtime;

	long diff = 0;


	do {
		usleep(50000);	//50 milliseconds
		g_get_current_time(&now);

		diff = (then->tv_sec - now.tv_sec)*1000000 + (then->tv_usec - now.tv_usec);
	} while (diff > 0);

	motoroff();
	g_free(motor_preview_killtime);

	motor_preview_thread = NULL;
	motor_preview_killtime = NULL;

	return NULL;
}

void pod_updatecal(const char * name, const char type, void * newvalue, void * target, const int justpreview)
{
	if (type != T_INTEGER)
	{
		LOG(LOG_WARN, "Unknown calibration entry: %s", name);
		return;
	}

	LOG(LOG_INFO, "Calibration preview %s", name);

	if (justpreview)
	{
		if (g_str_has_prefix(name, "motor"))
		{
			/*
			if (motor_preview_killtime == NULL)
			{
				motor_preview_killtime = g_malloc0(sizeof(GTimeVal));
			}

			g_get_current_time(motor_preview_killtime);
			g_time_val_add(motor_preview_killtime, 1500 * 1000); //1.5 sec

			if (motor_preview_thread == NULL)
			{
				GError * err = NULL;
				g_thread_create(pod_updatecal_motor, NULL, false, &err);
				if (err != NULL)
				{
					LOG(LOG_WARN, "Error while creating motor preview thread: %s", err->message);
					g_error_free(err);
				}

				usleep(50); //sleep a bit to ensure that motor_preview_thread gets set
			}

			if (motor_preview_thread != NULL)
			{
				pod_write_int(PCMD_PWMBASE + motor_front_channel, *(int *)newvalue);
				pod_write_int(PCMD_PWMBASE + motor_rear_channel, *(int *)newvalue);
			}
			*/
			pod_write_int(PCMD_PWMBASE + motor_front_channel, *(int *)newvalue);
			pod_write_int(PCMD_PWMBASE + motor_rear_channel, -*(int *)newvalue);
			motorpreview_expire = kernel_timestamp() + POD_MOTORPREVIEW_TIME;
		}
		else if (g_str_has_prefix(name, "front")) { pod_write_int(PCMD_PWMBASE + front_channel, *(int *)newvalue); }
		else if (g_str_has_prefix(name, "rear")) { pod_write_int(PCMD_PWMBASE + rear_channel, *(int *)newvalue); }
		else if (g_str_has_prefix(name, "pan")) { pod_write_int(PCMD_PWMBASE + pan_channel, *(int *)newvalue); }
		else if (g_str_has_prefix(name, "tilt")) { pod_write_int(PCMD_PWMBASE + tilt_channel, *(int *)newvalue); }
	}
	else
	{
		memcpy(target, newvalue, sizeof(int));
		setmaps();
	}
}

/* ------------- INIT --------------------*/
void mod_init() {
	QUEUE_INIT(&queue, data, sizeof(data));

	pod_fd = serial_open(serial_port, B115200);
	mainloop_addwatch(NULL, pod_fd, FD_READ, pod_newdata, NULL);

	setmaps();
	kthread_newinterval("MaxPOD Heartbeat", KTH_PRIO_LOW, 3.0, pod_heartbeat, NULL);
	kthread_newinterval("MaxPOD Motor Calibration Preview", KTH_PRIO_LOW, 2.0, pod_motorpreview, NULL);
}

void mod_destroy()
{
	//turn off everything
	alloff();
}

static void setmaps()
{
	//motor
	if (map_motor != NULL) map_destroy(map_motor);
	double motor_from[] = {-1.0, 0.0, 1.0};
	double motor_to[] = {motor_min, motor_center, motor_max};
	map_motor = map_newfromarray(MAP_LINEAR, motor_from, motor_to, 3);
	if (map_motor_rev != NULL) map_destroy(map_motor_rev);
	map_motor_rev = map_reverse(map_motor);

	//front
	if (map_front != NULL) map_destroy(map_front);
	double front_from[] = {-5.0*M_PI/36.0, 0.0, 5.0*M_PI/36.0};
	double front_to[] = {front_min, front_center, front_max};
	map_front = map_newfromarray(MAP_LINEAR, front_from, front_to, 3);
	if (map_front_rev != NULL) map_destroy(map_front_rev);
	map_front_rev = map_reverse(map_front);

	//rear
	if (map_rear != NULL) map_destroy(map_rear);
	double rear_from[] = {-5.0*M_PI/36.0, 0.0, 5.0*M_PI/36.0};
	double rear_to[] = {rear_min, rear_center, rear_max};
	map_rear = map_newfromarray(MAP_LINEAR, rear_from, rear_to, 3);
	if (map_rear_rev != NULL) map_destroy(map_rear_rev);
	map_rear_rev = map_reverse(map_rear);

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
