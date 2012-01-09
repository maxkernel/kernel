#include <string.h>
#include <unistd.h>

#include <kernel.h>
#include <aul/serial.h>
#include <aul/queue.h>
#include <aul/mainloop.h>

MOD_VERSION("1.0");
MOD_AUTHOR("Andrew Klofas <andrew@maxkernel.com>");
MOD_DESCRIPTION("Driver for MaxPOD (Forth) IO Board.");
MOD_INIT(maxpod_init);

CFG_PARAM(	serial_port,			"s");

BLK_INPUT(STATIC, pwm0, "i");
BLK_INPUT(STATIC, pwm1, "i");
BLK_INPUT(STATIC, pwm2, "i");
BLK_INPUT(STATIC, pwm3, "i");
BLK_INPUT(STATIC, pwm4, "i");
BLK_INPUT(STATIC, pwm5, "i");
BLK_ONUPDATE(STATIC, maxpod_update);


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


#define QUEUE_SIZE					(PACKET_SIZE * 5)
#define MAXPOD_HEARTBEAT			(NANOS_PER_SECOND / 2)

static int pod_fd = -1;
static queue_t queue;
static char data[QUEUE_SIZE];
char * serial_port				= "/dev/ttyS0";

#if 0
static map_t * map_pan				= NULL;
static map_t * map_tilt				= NULL;
#endif

static inline void maxpod_write(uint8_t cmd, uint8_t param1, uint8_t param2)
{
	if (pod_fd == -1)
	{
		LOG1(LOG_WARN, "Attempting to access MaxPOD when not connected!");
		return;
	}

	char data[6] = {STX, param1, param2, cmd, (param1+param2+cmd)%256, ETX};
	write(pod_fd, data, sizeof(data));
}

static inline void maxpod_write_padding()
{
	if (pod_fd == -1)
		return;

	char data[6];
	memset(data, DLE, sizeof(data));
	write(pod_fd, data, sizeof(data));
}

static inline void maxpod_write_int16(uint8_t cmd, uint16_t param)
{
	maxpod_write(cmd, (param & 0xFF00)>>8, param & 0xFF);
}

static bool maxpod_heartbeat(mainloop_t * loop, uint64_t nanoseconds, void * userdata)
{
	if (pod_fd == -1)
		return false;

	maxpod_write(PCMD_HEARTBEAT, 0, 0);

	return true;
}


static bool maxpod_newdata(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	static bool pod_initial_response = false;

	char d;
	ssize_t numread;

	// Read one byte from serial
	numread = read(fd, &d, sizeof(char));
	if (numread != sizeof(char))
	{
		//end stream!
		LOG(LOG_WARN, "End of stream reached in MaxPOD serial port %s", serial_port);
		return false;
	}

	// Queue it up
	queue_enqueue(&queue, &d, sizeof(char));

	// Strip out the framing
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
			maxpod_write(PCMD_ERROR, PERR_MALFORMED_CMD, 0);
		}

		if ( (packet[0]+packet[1]+packet[2]) % 256 != packet[3] )
		{
			//checksum bad
			LOG(LOG_WARN, "Checksum error in MaxPOD");
			maxpod_write(PCMD_ERROR, PERR_MALFORMED_CMD, 0);
		}

		pod_initial_response = true;

		switch (packet[2])
		{
			case PCMD_HEARTBEAT:
			{
				// Don't care about heartbeats
				break;
			}

			case PCMD_ECHO:
			{
				// TODO - calculate latency and output it
				break;
			}

			case PCMD_ERROR:
				LOG(LOG_ERR, "Error received from MaxPOD (opcode=%d, param=%d)", packet[0], packet[1]);
				maxpod_write_padding();
				break;

			default:
				LOG(LOG_WARN, "Unknown command received from MaxPOD");
				break;
		}
	}

	return true;
}

#if 0
DEF_SYSCALL(	pan,				"v:d",		"Rotates the head by given value (param 1) radians between -3pi/2 to pi/2 (-270 degrees to +90 degrees)");
DEF_SYSCALL(	tilt,				"v:d",		"Tilts the head by given value (param 1) radians between -pi/4 ... pi/2 (-45 degrees to +90 degrees)");
#endif

#if 0
void motor(double value)
{
	motor_throttle = CLAMP(value, -1.0, 1.0);

	if (enable_slowreverse && motor_throttle < 0.0)
	{
		//slow down reverse by 1/4
		motor_throttle *= 0.75;
	}

	maxpod_write_int16(PCMD_PWMBASE + motor_front_channel, (int)map_tovalue(map_motor, motor_throttle * direction));
	maxpod_write_int16(PCMD_PWMBASE + motor_rear_channel, (int)map_tovalue(map_motor_rev, motor_throttle * direction));
}


void turnfront(double value)
{
	//use reverse map if the direction is -1, plus invert the value
	map_t * map = direction == 1? map_front : map_front_rev;
	value *= direction;

	maxpod_write_int16(PCMD_PWMBASE + front_channel, (int)map_tovalue(map, CLAMP(value, -5.0*M_PI/36.0, 5.0*M_PI/36.0)));
}

void turnrear(double value)
{
	map_t * map = direction == 1? map_rear : map_rear_rev;
	value *= direction;

	maxpod_write_int16(PCMD_PWMBASE + rear_channel, (int)map_tovalue(map, CLAMP(value, -5.0*M_PI/36.0, 5.0*M_PI/36.0)));
}
#endif

#if 0
void pan(double value)
{
	while (value > M_PI) { value -= 2.0*M_PI; }
	while (value < -M_PI) { value += 2.0*M_PI; }

	maxpod_write_int16(PCMD_PWMBASE + pan_channel, (int)map_tovalue(map_pan, value));
}

void tilt(double value)
{
	maxpod_write_int16(PCMD_PWMBASE + tilt_channel, (int)map_tovalue(map_tilt, value));
}
#endif

#if 0
void pod_previewcal(const char * name, const char type, void * newvalue, void * target)
{
	if (type != T_INTEGER)
	{
		LOG(LOG_WARN, "Unknown calibration entry: %s", name);
		return;
	}

	LOG(LOG_INFO, "Calibration preview %s", name);

	if (g_str_has_prefix(name, "pan")) { maxpod_write_int16(PCMD_PWMBASE + pan_channel, *(int *)newvalue); }
	else if (g_str_has_prefix(name, "tilt")) { maxpod_write_int16(PCMD_PWMBASE + tilt_channel, *(int *)newvalue); }
}

void pod_updatecal(const char * name, const char type, void * newvalue, void * target)
{
	if (type != T_INTEGER)
	{
		LOG(LOG_WARN, "Unknown calibration entry: %s", name);
		return;
	}

	LOG(LOG_INFO, "Calibration save %s", name);

	memcpy(target, newvalue, sizeof(int));
	setmaps();
}
#endif


void maxpod_update(void * obj)
{
	if (pod_fd == -1)
	{
		// File descriptor invalid
		return;
	}

	// Get the input
	const int * pwm0 = INPUT(pwm0);
	const int * pwm1 = INPUT(pwm1);
	const int * pwm2 = INPUT(pwm2);
	const int * pwm3 = INPUT(pwm3);
	const int * pwm4 = INPUT(pwm4);
	const int * pwm5 = INPUT(pwm5);

	// Now write it to the servos
	if (pwm0 != NULL)	maxpod_write_int16(PCMD_PWMBASE + 0, *pwm0);
	if (pwm1 != NULL)	maxpod_write_int16(PCMD_PWMBASE + 1, *pwm1);
	if (pwm2 != NULL)	maxpod_write_int16(PCMD_PWMBASE + 2, *pwm2);
	if (pwm3 != NULL)	maxpod_write_int16(PCMD_PWMBASE + 3, *pwm3);
	if (pwm4 != NULL)	maxpod_write_int16(PCMD_PWMBASE + 4, *pwm4);
	if (pwm5 != NULL)	maxpod_write_int16(PCMD_PWMBASE + 5, *pwm5);
}


void maxpod_init() {
	QUEUE_INIT(&queue, data, sizeof(data));

	pod_fd = serial_open(serial_port, B115200);
	mainloop_addwatch(NULL, pod_fd, FD_READ, maxpod_newdata, NULL);
	mainloop_addtimer(NULL, "MaxPOD Heartbeat", MAXPOD_HEARTBEAT, maxpod_heartbeat, NULL);
}
