#include <unistd.h>
#include <stddef.h>
#include <string.h>

#include <aul/string.h>
#include <aul/serial.h>
#include <aul/mainloop.h>
#include <kernel.h>


#define NUM_CHANNELS	16
#define BUFFER_LENGTH	50
#define PCMD_LENGTH		8

#define PCMD_VERSION	"!SCVER?\r"
#define PCMD_SETBAUD	"!SCSBR\x01\r"
#define PCMD_SETPOS		"!SCxxxx\r"
#define PCMD_ENABLE		"!SCPSEx\r"
#define PCMD_DISABLE	"!SCPSDx\r"

#define PLEN_VERSION	3
#define PLEN_SETBAUD	3

typedef enum
{
	PS_NONE		= 'N',
	PS_SETBAUD	= 'B',
	PS_VERSION	= 'V',
} rxstate_t;

typedef struct
{
	int fd;
	rxstate_t state;
	int ramp;

	char buffer[BUFFER_LENGTH];
	size_t index;
} pololu_t;


static void pololu_write(pololu_t * p, rxstate_t newstate, char * cmd)
{
	ssize_t bytes = write(p->fd, cmd, PCMD_LENGTH);
	if (bytes != PCMD_LENGTH)
	{
		LOG1(LOG_ERR, "Could not write all bytes to Propeller SSC");
		return;
	}
	
	if (newstate == PS_NONE)
		return;
	
	if (p->state != PS_NONE)
	{
		LOG(LOG_WARN, "Overwriting Propeller SSC state. Current state: %c, new state: %c", p->state, newstate);
	}
	
	p->state = newstate;
}

static void pololu_enableio(pololu_t * p, int channel)
{
	char cmd[PCMD_LENGTH];
	memcpy(cmd, PCMD_ENABLE, PCMD_LENGTH);
	cmd[6] = (char)channel;
	pololu_write(p, PS_NONE, cmd);
}

static void pololu_disableio(pololu_t * p, int channel)
{
	char cmd[PCMD_LENGTH];
	memcpy(cmd, PCMD_DISABLE, PCMD_LENGTH);
	cmd[6] = (char)channel;
	pololu_write(p, PS_NONE, cmd);
}

static void pololu_disableall(pololu_t * p)
{
	for (int i = 0; i < NUM_CHANNELS; i++)
	{
		pololu_disableio(p, i);
	}
}

static void pololu_writeio(pololu_t * p, int channel, int value)
{
	pololu_enableio(p, channel);

	char cmd[PCMD_LENGTH];
	memcpy(cmd, PCMD_SETPOS, PCMD_LENGTH);
	cmd[3] = (char)channel;
	cmd[4] = (char)p->ramp;
	cmd[5] = (char)(value & 0xFF);
	cmd[6] = (char)((value & 0xFF00) >> 8);
	pololu_write(p, PS_NONE, cmd);
}

static bool pololu_newdata(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	pololu_t * p = userdata;
	
	ssize_t bytes = read(p->fd, p->buffer + p->index, BUFFER_LENGTH - p->index);
	if (bytes <= 0)
	{
		//end stream!
		LOG(LOG_ERR, "End of stream reached in Pololu SSC");
		return false;
	}
	
	p->index += bytes;
	
	switch (p->state)
	{
		case PS_SETBAUD:
		{
			if (p->index < PLEN_SETBAUD)
			{
				break;
			}

			char newbaud[PLEN_SETBAUD + 1] = {0};
			memcpy(newbaud, p->buffer, PLEN_SETBAUD);
			LOG(LOG_INFO, "Successful baud rate selection to mode %d on Parallax SSC", newbaud[2]);

			memmove(p->buffer, p->buffer + PLEN_SETBAUD, p->index -= PLEN_SETBAUD);
			p->state = PS_NONE;
			break;
		}

		case PS_VERSION:
		{
			if (p->index < PLEN_VERSION)
			{
				break;
			}
			
			char version[PLEN_VERSION + 1] = {0};
			memcpy(version, p->buffer, PLEN_VERSION);
			LOG(LOG_INFO, "Connected to Parallax SSC version %s", version);
			
			memmove(p->buffer, p->buffer + PLEN_VERSION, p->index -= PLEN_VERSION);
			p->state = PS_NONE;

			break;
		}
		
		case PS_NONE:
		{
			LOG(LOG_WARN, "Unexpected data received from Parallax SSC");
			p->index = 0;
		}
	}
	
	return true;
}

static void pololu_update(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	pololu_t * p = object;

	// Get the input
	const int * pwm0 = INPUT(pwm0);
	const int * pwm1 = INPUT(pwm1);
	const int * pwm2 = INPUT(pwm2);
	const int * pwm3 = INPUT(pwm3);
	const int * pwm4 = INPUT(pwm4);
	const int * pwm5 = INPUT(pwm5);
	const int * pwm6 = INPUT(pwm6);
	const int * pwm7 = INPUT(pwm7);
	const int * pwm8 = INPUT(pwm8);
	const int * pwm9 = INPUT(pwm9);
	const int * pwm10 = INPUT(pwm10);
	const int * pwm11 = INPUT(pwm11);
	const int * pwm12 = INPUT(pwm12);
	const int * pwm13 = INPUT(pwm13);
	const int * pwm14 = INPUT(pwm14);
	const int * pwm15 = INPUT(pwm15);
	

	// Now write it to the servos
	if (pwm0 != NULL)	pololu_writeio(p, 0, *pwm0);		else	pololu_disableio(p, 0);
	if (pwm1 != NULL)	pololu_writeio(p, 1, *pwm1);		else	pololu_disableio(p, 1);
	if (pwm2 != NULL)	pololu_writeio(p, 2, *pwm2);		else	pololu_disableio(p, 2);
	if (pwm3 != NULL)	pololu_writeio(p, 3, *pwm3);		else	pololu_disableio(p, 3);
	if (pwm4 != NULL)	pololu_writeio(p, 4, *pwm4);		else	pololu_disableio(p, 4);
	if (pwm5 != NULL)	pololu_writeio(p, 5, *pwm5);		else	pololu_disableio(p, 5);
	if (pwm6 != NULL)	pololu_writeio(p, 6, *pwm6);		else	pololu_disableio(p, 6);
	if (pwm7 != NULL)	pololu_writeio(p, 7, *pwm7);		else	pololu_disableio(p, 7);
	if (pwm8 != NULL)	pololu_writeio(p, 8, *pwm8);		else	pololu_disableio(p, 8);
	if (pwm9 != NULL)	pololu_writeio(p, 9, *pwm9);		else	pololu_disableio(p, 9);
	if (pwm10 != NULL)	pololu_writeio(p, 10, *pwm10);		else	pololu_disableio(p, 10);
	if (pwm11 != NULL)	pololu_writeio(p, 11, *pwm11);		else	pololu_disableio(p, 11);
	if (pwm12 != NULL)	pololu_writeio(p, 12, *pwm12);		else	pololu_disableio(p, 12);
	if (pwm13 != NULL)	pololu_writeio(p, 13, *pwm13);		else	pololu_disableio(p, 13);
	if (pwm14 != NULL)	pololu_writeio(p, 14, *pwm14);		else	pololu_disableio(p, 14);
	if (pwm15 != NULL)	pololu_writeio(p, 15, *pwm15);		else	pololu_disableio(p, 15);
}

static void * pololu_new(const char * serial_port, int baud)
{
	pololu_t * p = malloc(sizeof(pololu_t));
	memset(p, 0, sizeof(pololu_t));

	p->state = PS_NONE;
	p->ramp = 7;

	p->fd = serial_open(serial_port, serial_getspeed(baud));
	if (p->fd < 0)
	{
		LOG(LOG_ERR, "Could not open Parallax SSC serial port %s", serial_port);
		return false;
	}

	// Write the set-baud command and wait for a bit
	write(p->fd, PCMD_SETBAUD, PCMD_LENGTH);
	usleep(50000);

	// Change the serial port speed
	serial_setattr(p->fd, B38400);

	// Register the FD and send a version request
	mainloop_addwatch(NULL, p->fd, FD_READ, pololu_newdata, p);
	pololu_disableall(p);
	pololu_write(p, PS_VERSION, PCMD_VERSION);

	return p;
}

static void pololu_destroy(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	pololu_t * p = object;
	pololu_disableall(p);
	serial_flush(p->fd);
	close(p->fd);
	free(p);
}


define_block(	pololu,		"Control a Pololu SSC", pololu_new, "si", "(1) serial port [eg. /dev/ttyS0] (2) Baud rate [eg. 2400]");
block_onupdate(	pololu, 	pololu_update);
block_ondestroy(pololu,		pololu_destroy);
block_input(	pololu, 	pwm0, 		'i', 	"Channel 0 PWM");
block_input(	pololu, 	pwm1, 		'i', 	"Channel 1 PWM");
block_input(	pololu, 	pwm2, 		'i', 	"Channel 2 PWM");
block_input(	pololu, 	pwm3, 		'i', 	"Channel 3 PWM");
block_input(	pololu, 	pwm4, 		'i', 	"Channel 4 PWM");
block_input(	pololu, 	pwm5, 		'i', 	"Channel 5 PWM");
block_input(	pololu, 	pwm6, 		'i', 	"Channel 6 PWM");
block_input(	pololu, 	pwm7, 		'i', 	"Channel 7 PWM");
block_input(	pololu, 	pwm8, 		'i', 	"Channel 8 PWM");
block_input(	pololu, 	pwm9, 		'i', 	"Channel 9 PWM");
block_input(	pololu, 	pwm10, 		'i', 	"Channel 10 PWM");
block_input(	pololu, 	pwm11, 		'i', 	"Channel 11 PWM");
block_input(	pololu, 	pwm12, 		'i', 	"Channel 12 PWM");
block_input(	pololu, 	pwm13, 		'i', 	"Channel 13 PWM");
block_input(	pololu, 	pwm14, 		'i', 	"Channel 14 PWM");
block_input(	pololu, 	pwm15, 		'i', 	"Channel 15 PWM");
