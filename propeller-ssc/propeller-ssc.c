#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#include <aul/string.h>
#include <aul/serial.h>
#include <aul/mainloop.h>
#include <map.h>
#include <kernel.h>

// Module defines
MOD_VERSION("1.0");
MOD_AUTHOR("Andrew Klofas <aklofas@gmail.com>");
MOD_DESCRIPTION("Communicates to the Propeller Servo Controller USB");
MOD_DEPENDENCY("map");
MOD_INIT(module_init);
MOD_DESTROY(module_destroy);
BLK_ONUPDATE(STATIC, psc_update);
BLK_INPUT(STATIC, turnfront, S_DOUBLE, "Front turn angle in radians");
BLK_INPUT(STATIC, turnrear, S_DOUBLE, "Rear turn angle in radians");
BLK_INPUT(STATIC, pan, S_DOUBLE, "Pan the head the specified angle in radians");
BLK_INPUT(STATIC, tilt, S_DOUBLE, "Tilt the head the specified angle in radians");
BLK_INPUT(STATIC, motor, S_DOUBLE, "Energizes the motor by the specified percent (-1.0 to 1.0)");


// Configuration
char * serial_port 			= "/dev/ttyUSB0";
int servo_ramp				= 7;
int motor_front_channel		= 0;
int motor_rear_channel		= 1;
int front_channel			= 2;
int rear_channel			= 3;
int pan_channel				= 4;
int tilt_channel			= 5;

CFG_PARAM(serial_port,			"s");
CFG_PARAM(servo_ramp,			"i");
CFG_PARAM(motor_front_channel,	"i");
CFG_PARAM(motor_rear_channel,	"i");
CFG_PARAM(front_channel,		"i");
CFG_PARAM(rear_channel,			"i");
CFG_PARAM(pan_channel,			"i");
CFG_PARAM(tilt_channel,			"i");


// Calibration
int motor_min	= 800,		motor_center	= 800,		motor_max		= 800;
int front_min	= 900,		front_center	= 700,		front_max		= 500;
int rear_min	= 500,		rear_center		= 700,		rear_max		= 900;
int pan_min		= 800,		pan_forward		= 800,		pan_backward	= 800,		pan_max		= 800;
int tilt_min	= 800,		tilt_center		= 800,		tilt_max		= 800;

CAL_UPDATE(psc_updatecal);
CAL_PARAM(motor_min,	"i0:1500",	"Motor Minimum (full reverse)"			);
CAL_PARAM(motor_center,	"i0:1500",	"Motor Center (neutral)"				);
CAL_PARAM(motor_max,	"i0:1500",	"Motor Maximum (full forward)"			);
CAL_PARAM(front_min,	"i0:1500",	"Front Wheel Minimum (left -25 deg)"	);
CAL_PARAM(front_center,	"i0:1500",	"Front Wheel Center"					);
CAL_PARAM(front_max,	"i0:1500",	"Front Wheel Maximum (right +25 deg)"	);
CAL_PARAM(rear_min,		"i0:1500",	"Rear Wheel Minimum (right +25 deg)"	);
CAL_PARAM(rear_center,	"i0:1500",	"Rear Wheel Center"						);
CAL_PARAM(rear_max,		"i0:1500",	"Rear Wheel Maximum (left -25 deg)"		);
CAL_PARAM(pan_min,		"i0:1500",	"Head Pan Minimum (left -270 deg)"		);
CAL_PARAM(pan_forward,	"i0:1500",	"Head Pan Center Forward"				);
CAL_PARAM(pan_backward,	"i0:1500",	"Head Pan Center Backward"				);
CAL_PARAM(pan_max,		"i0:1500",	"Head Pan Maximum (right +90 deg)"		);
CAL_PARAM(tilt_min,		"i0:1500",	"Head Tilt Minimum (down -45 deg)"		);
CAL_PARAM(tilt_center,	"i0:1500",	"Head Tilt Center (level forward)"		);
CAL_PARAM(tilt_max,		"i0:1500",	"Head Tilt Maximum (up +90 deg)"		);

// TODO - delete me!
DEF_SYSCALL(calmode,				"v:i",		"sets the calibration mode");
static int cal_mode = 0;
void calmode(int mode)
{
	LOG(LOG_INFO, "<DELETE ME> Setting cal mode to %d", mode);
	cal_mode = mode;
}


#define BUFFER_LENGTH	50
#define PCMD_LENGTH		8

#define PCMD_VERSION	"!SCVER?\r"
#define PCMD_SETPOS		"!SCxxxx\r"

#define PLEN_VERSION	3

typedef enum
{
	PS_NONE		= 'N',
	PS_VERSION	= 'V',
} rxstate_t;


static void setmaps();

static int pfd = -1;
static rxstate_t state = PS_NONE;
static bool write_error = false;

static map_t * map_motor			= NULL;
static map_t * map_motor_rev		= NULL;
static map_t * map_front			= NULL;
static map_t * map_rear				= NULL;
static map_t * map_pan				= NULL;
static map_t * map_tilt				= NULL;

static void psc_write(rxstate_t newstate, char * cmd)
{
	if (write_error)
		return;
	
	if (pfd == -1)
	{
		LOG(LOG_ERR, "Unable to write to Propeller SSC, invalid file descriptor");
		write_error = true;
		return;
	}
	
	ssize_t bytes = write(pfd, cmd, PCMD_LENGTH);
	if (bytes != PCMD_LENGTH)
	{
		LOG(LOG_WARN, "Could not write all bytes to Propeller SSC %s", serial_port);
		return;
	}
	
	if (newstate == PS_NONE)
		return;
	
	if (state != PS_NONE)
	{
		LOG(LOG_WARN, "Overwriting Propeller SSC state. Current state: %c, new state: %c", state, newstate);
	}
	
	state = newstate;
}

static void psc_setpos(int channel, int value)
{
	char cmd[PCMD_LENGTH];
	memcpy(cmd, PCMD_SETPOS, PCMD_LENGTH);
	
	cmd[3] = (char)channel;
	cmd[4] = (char)servo_ramp;
	cmd[5] = (char)(value & 0xFF);
	cmd[6] = (char)((value & 0xFF00) >> 8);
	
	psc_write(PS_NONE, cmd);
}

static bool psc_newdata(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	static char buffer[BUFFER_LENGTH];
	static size_t index = 0;
	
	ssize_t bytes = read(pfd, buffer + index, BUFFER_LENGTH - index);
	if (bytes <= 0)
	{
		//end stream!
		LOG(LOG_ERR, "End of stream reached in Propeller SSC %s", serial_port);
		write_error = true;
		return false;
	}
	
	index += bytes;
	
	switch (state)
	{
		case PS_VERSION:
		{
			if (index < PLEN_VERSION)
			{
				break;
			}
			
			char version[PLEN_VERSION + 1] = {0};
			memcpy(version, buffer, PLEN_VERSION);
			LOG(LOG_INFO, "Connected to Propeller SSC version %s", version);
			
			memmove(buffer, buffer + PLEN_VERSION, index -= PLEN_VERSION);
			state = PS_NONE;
			break;
		}
		
		case PS_NONE:
		{
			LOG(LOG_WARN, "Unexpected data received from Propeller SSC %s", serial_port);
			index = 0;
		}
	}
	
	return true;
}

void psc_updatecal(const char * name, const char type, void * newvalue, void * target, const int justpreview)
{
	if (type != T_INTEGER)
	{
		LOG(LOG_WARN, "Unknown calibration entry: %s", name);
		return;
	}

	LOG(LOG_INFO, "Calibration preview %s", name);

	if (justpreview)
	{
		if (strprefix(name, "motor"))
		{
			psc_setpos(motor_front_channel, *(int *)newvalue);
			psc_setpos(motor_front_channel, -*(int *)newvalue);
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
			//pod_write_int(PCMD_PWMBASE + motor_front_channel, *(int *)newvalue);
			//pod_write_int(PCMD_PWMBASE + motor_rear_channel, -*(int *)newvalue);
			//motorpreview_expire = kernel_timestamp() + POD_MOTORPREVIEW_TIME;
		}
		else if (strprefix(name, "front")) { psc_setpos(front_channel, *(int *)newvalue); }
		else if (strprefix(name, "rear")) { psc_setpos(rear_channel, *(int *)newvalue); }
		else if (strprefix(name, "pan")) { psc_setpos(pan_channel, *(int *)newvalue); }
		else if (strprefix(name, "tilt")) { psc_setpos(tilt_channel, *(int *)newvalue); }
	}
	else
	{
		memcpy(target, newvalue, sizeof(int));
	}
}

void psc_update(void * object)
{
	if (cal_mode)
	{
		return;
	}

	if (!ISNULL(motor))
	{
		double motor = (double) CLAMP(INPUTT(double, motor), -1.0, 1.0);
		psc_setpos(motor_front_channel, (int)map_tovalue(map_motor, motor));
		psc_setpos(motor_rear_channel, (int)map_tovalue(map_motor_rev, motor));
	}

	if (!ISNULL(turnfront))
	{
		double front = (double) CLAMP(INPUTT(double, turnfront), -5.0*M_PI/36.0, 5.0*M_PI/36.0);
		psc_setpos(front_channel, (int)map_tovalue(map_front, front));
	}

	if (!ISNULL(turnrear))
	{
		double rear = (double) CLAMP(INPUTT(double, turnrear), -5.0*M_PI/36.0, 5.0*M_PI/36.0);
		psc_setpos(rear_channel, (int)map_tovalue(map_rear, rear));
	}

	if (!ISNULL(pan))
	{
		double pan = (double) INPUTT(double, pan);
		while (pan > M_PI) { pan -= 2.0*M_PI; }
		while (pan < -M_PI) { pan += 2.0*M_PI; }

		psc_setpos(pan_channel, (int)map_tovalue(map_pan, pan));
	}

	if (!ISNULL(tilt))
	{
		double tilt = (double) INPUTT(double, tilt);
		psc_setpos(tilt_channel, (int)map_tovalue(map_tilt, tilt));
	}
}

void module_init() {
	setmaps();

	pfd = serial_open(serial_port, B2400);
	if (pfd < 0)
	{
		LOG(LOG_ERR, "Could not open Propeller SSC serial port %s", serial_port);
		return;
	}
	
	mainloop_addwatch(NULL, pfd, FD_READ, psc_newdata, NULL);
	psc_write(PS_VERSION, PCMD_VERSION);
}

void module_destroy()
{
	if (pfd != -1)
	{
		close(pfd);
	}
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
