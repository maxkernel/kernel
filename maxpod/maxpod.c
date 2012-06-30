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


typedef struct
{
	fdwatcher_t watcher;
	timerwatcher_t timer;
	bool hasresponded;
	queue_t queue;
	char data[QUEUE_SIZE];
} maxpod_t;


static inline void maxpod_write(const maxpod_t * maxpod, uint8_t cmd, uint8_t param1, uint8_t param2)
{
	char data[6] = {STX, param1, param2, cmd, (param1+param2+cmd)%256, ETX};
	write(watcher_fd(&maxpod->watcher), data, sizeof(data));
}

static inline void maxpod_write_padding(const maxpod_t * maxpod)
{
	char data[6];
	memset(data, DLE, sizeof(data));
	write(watcher_fd(&maxpod->watcher), data, sizeof(data));
}

static inline void maxpod_write_int16(const maxpod_t * maxpod, uint8_t cmd, uint16_t param)
{
	maxpod_write(maxpod, cmd, (param & 0xFF00)>>8, param & 0xFF);
}

static bool maxpod_heartbeat(mainloop_t * loop, uint64_t nanoseconds, void * userdata)
{
	const maxpod_t * maxpod = userdata;
	maxpod_write(maxpod, PCMD_HEARTBEAT, 0, 0);
	return true;
}


static bool maxpod_newdata(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	labels(end);

	maxpod_t * maxpod = userdata;

	// Read one byte from serial
	{
		char data = 0;
		ssize_t numread = read(fd, &data, sizeof(char));
		if (numread != sizeof(char))
		{
			//end stream!
			LOG(LOG_WARN, "End of stream reached in MaxPOD serial port");
			return false;
		}

		// Queue it up
		queue_enqueue(&maxpod->queue, &data, sizeof(char));
	}

	// Strip out the framing
	while (true)
	{
		char data = 0;

		if (queue_size(&maxpod->queue) < PACKET_SIZE)
		{
			// We don't have a full packet
			goto end;
		}

		//dump all DLE's not in a packet
		queue_dequeue(&maxpod->queue, &data, sizeof(char));

		if (data == STX)
		{
			break;
		}
		else if (data != DLE)
		{
			LOG(LOG_WARN, "Out of sync byte read from MaxPOD, expected DLE (%d), got %d", DLE, data);
		}
	}

	// Parse packet
	{
		int packet[PACKET_SIZE - 1];

		queue_dequeue(&maxpod->queue, packet, sizeof(packet));

		// Frame error
		if (packet[4] != ETX)
		{
			if (maxpod->hasresponded)
			{
				LOG(LOG_WARN, "Frame error in MaxPOD");
				maxpod_write(maxpod, PCMD_ERROR, PERR_MALFORMED_CMD, 0);
			}
			goto end;
		}

		if ( (packet[0]+packet[1]+packet[2]) % 256 != packet[3] )
		{
			// Bad checksum
			LOG(LOG_WARN, "Checksum error in MaxPOD");
			maxpod_write(maxpod, PCMD_ERROR, PERR_MALFORMED_CMD, 0);
			goto end;
		}

		maxpod->hasresponded = true;

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
				maxpod_write_padding(maxpod);
				break;

			default:
				LOG(LOG_WARN, "Unknown command received from MaxPOD");
				break;
		}
	}
end:

	return true;
}

static void maxpod_update(void * object)
{
	maxpod_t * maxpod = object;
	if (maxpod == NULL)
	{
		return;
	}

	// Get the input
	const int * pwm0 = input(pwm0);
	const int * pwm1 = input(pwm1);
	const int * pwm2 = input(pwm2);
	const int * pwm3 = input(pwm3);
	const int * pwm4 = input(pwm4);
	const int * pwm5 = input(pwm5);

	// Now write it to the servos
	if (pwm0 != NULL)	maxpod_write_int16(maxpod, PCMD_PWMBASE + 0, *pwm0);
	if (pwm1 != NULL)	maxpod_write_int16(maxpod, PCMD_PWMBASE + 1, *pwm1);
	if (pwm2 != NULL)	maxpod_write_int16(maxpod, PCMD_PWMBASE + 2, *pwm2);
	if (pwm3 != NULL)	maxpod_write_int16(maxpod, PCMD_PWMBASE + 3, *pwm3);
	if (pwm4 != NULL)	maxpod_write_int16(maxpod, PCMD_PWMBASE + 4, *pwm4);
	if (pwm5 != NULL)	maxpod_write_int16(maxpod, PCMD_PWMBASE + 5, *pwm5);
}


static void * maxpod_new(const char * serial_port)
{
	labels(after_timer);

	int fd = serial_open(serial_port, B115200);
	if (fd == -1)
	{
		LOG(LOG_ERR, "Could not open serial port %s", serial_port);
		return NULL;
	}

	maxpod_t * maxpod = malloc(sizeof(maxpod_t));
	memset(maxpod, 0, sizeof(maxpod_t));
	maxpod->hasresponded = false;
	queue_init(&maxpod->queue, maxpod->data, QUEUE_SIZE);

	// Add fd to mainloop
	{
		exception_t * e = NULL;
		watcher_newfd(&maxpod->watcher, fd, FD_READ, maxpod_newdata, maxpod);
		if (!mainloop_addwatcher(kernel_mainloop(), &maxpod->watcher, &e) || exception_check(&e))
		{
			LOG(LOG_ERR, "Could not add maxpod fd to mainloop: %s", exception_message(e));
			exception_free(e);

			free(maxpod);
			return NULL;
		}
	}

	// Create a heartbeat timerfd
	{
		exception_t * e = NULL;
		if (!watcher_newtimer(&maxpod->timer, "MaxPOD Heartbeat", MAXPOD_HEARTBEAT, maxpod_heartbeat, maxpod, &e) || exception_check(&e))
		{
			LOG(LOG_ERR, "Could not create heartbeat timerfd: %s", exception_message(e));
			exception_free(e);

			goto after_timer;
		}

		if (!mainloop_addwatcher(kernel_mainloop(), watcher_cast(&maxpod->timer), &e) || exception_check(&e))
		{
			LOG(LOG_ERR, "Could not add maxpod heartbeat timerfd to mainloop: %s", exception_message(e));
			exception_free(e);

			// Don't free maxpod here, we've already set up the serial watcher
			// Just let the heartbeat timeout and shut off the motors
			goto after_timer;
		}
	}
after_timer:

	return maxpod;
}

static void maxpod_destroy(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	maxpod_t * maxpod = object;

	// Destroy watcher
	{
		exception_t * e = NULL;
		if (!mainloop_removewatcher(&maxpod->watcher, &e) || exception_check(&e))
		{
			LOG(LOG_WARN, "Could not remove maxpod watch: %s", exception_message(e));
			exception_free(e);
		}
	}

	// Destroy timer watcher
	{
		exception_t * e = NULL;
		if (!mainloop_removewatcher(watcher_cast(&maxpod->timer), &e) || exception_check(&e))
		{
			LOG(LOG_WARN, "Could not remove maxpod timer: %s", exception_message(e));
			exception_free(e);
		}
	}

	free(maxpod);
}


module_name("MaxPOD PWM Microcontroller");
module_version(1,0,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Driver for MaxPOD (Forth) IO Board.");

define_block(	pod,	"Connect to a maxpod IO board", maxpod_new, "s", "(1) The serial port to connect to [eg. /dev/ttyS0]");
block_onupdate(	pod, 	maxpod_update);
block_ondestroy(pod,	maxpod_destroy);
block_input(	pod, 	pwm0, 	'i', 	"Channel 0 PWM");
block_input(	pod, 	pwm1, 	'i', 	"Channel 1 PWM");
block_input(	pod, 	pwm2, 	'i', 	"Channel 2 PWM");
block_input(	pod, 	pwm3, 	'i', 	"Channel 3 PWM");
block_input(	pod, 	pwm4, 	'i', 	"Channel 4 PWM");
block_input(	pod, 	pwm5, 	'i', 	"Channel 5 PWM");
