#include <string.h>
#include <unistd.h>

#include <kernel.h>
#include <aul/serial.h>
#include <aul/queue.h>
#include <aul/mainloop.h>


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


bool maxpod_init() {
	queue_init(&queue, data, sizeof(data));

	pod_fd = serial_open(serial_port, B115200);
	if (pod_fd == -1)
	{
		LOG(LOG_ERR, "Could not open serial port %s", serial_port);
		return false;
	}

	mainloop_addwatch(NULL, pod_fd, FD_READ, maxpod_newdata, NULL);
	mainloop_addtimer(NULL, "MaxPOD Heartbeat", MAXPOD_HEARTBEAT, maxpod_heartbeat, NULL);

	return true;
}


module_name("MaxPOD PWM Microcontroller");
module_version(1,0,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Driver for MaxPOD (Forth) IO Board.");
module_oninitialize(maxpod_init);

module_config(serial_port, 's', "The serial port to connect to (eg /dev/ttyS0)");

block_input(	static, 	pwm0, 	'i', 	"Channel 0 PWM");
block_input(	static, 	pwm1, 	'i', 	"Channel 1 PWM");
block_input(	static, 	pwm2, 	'i', 	"Channel 2 PWM");
block_input(	static, 	pwm3, 	'i', 	"Channel 3 PWM");
block_input(	static, 	pwm4, 	'i', 	"Channel 4 PWM");
block_input(	static, 	pwm5, 	'i', 	"Channel 5 PWM");
block_onupdate(	static, 	maxpod_update);
