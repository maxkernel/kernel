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


static int psc_fd = -1;
static rxstate_t state = PS_NONE;
static bool channel_enabled[NUM_CHANNELS];
char * serial_port = "/dev/ttyUSB0";
int servo_ramp = 7;

static void psc_write(rxstate_t newstate, char * cmd)
{
	if (psc_fd == -1)
	{
		LOG1(LOG_ERR, "Unable to write to Propeller SSC, bad file descriptor");
		return;
	}
	
	ssize_t bytes = write(psc_fd, cmd, PCMD_LENGTH);
	if (bytes != PCMD_LENGTH)
	{
		LOG1(LOG_ERR, "Could not write all bytes to Propeller SSC %s", serial_port);
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

static void psc_enableio(int channel)
{
	if (channel_enabled[channel])
	{
		return;
	}

	char cmd[PCMD_LENGTH];
	memcpy(cmd, PCMD_ENABLE, PCMD_LENGTH);
	cmd[6] = (char)channel;
	psc_write(PS_NONE, cmd);

	channel_enabled[channel] = true;
}

static void psc_disableio(int channel)
{
	if (!channel_enabled[channel])
	{
		return;
	}

	char cmd[PCMD_LENGTH];
	memcpy(cmd, PCMD_DISABLE, PCMD_LENGTH);
	cmd[6] = (char)channel;
	psc_write(PS_NONE, cmd);

	channel_enabled[channel] = false;
}

static void psc_disableall()
{
	for (int i=0; i<NUM_CHANNELS; i++)
	{
		channel_enabled[i] = true;
		psc_disableio(i);
	}
}

static void psc_writeio(int channel, const int value)
{
	if (!channel_enabled[channel])
	{
		psc_enableio(channel);
	}

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
	
	ssize_t bytes = read(psc_fd, buffer + index, BUFFER_LENGTH - index);
	if (bytes <= 0)
	{
		//end stream!
		LOG(LOG_ERR, "End of stream reached in Propeller SSC %s", serial_port);
		return false;
	}
	
	index += bytes;
	
	switch (state)
	{
		case PS_SETBAUD:
		{
			if (index < PLEN_SETBAUD)
			{
				break;
			}

			char newbaud[PLEN_SETBAUD + 1] = {0};
			memcpy(newbaud, buffer, PLEN_SETBAUD);
			LOG(LOG_INFO, "Successful baud rate selection to mode %d on Parallax SSC", newbaud[2]);

			memmove(buffer, buffer + PLEN_SETBAUD, index -= PLEN_SETBAUD);
			state = PS_NONE;
			break;
		}

		case PS_VERSION:
		{
			if (index < PLEN_VERSION)
			{
				break;
			}
			
			char version[PLEN_VERSION + 1] = {0};
			memcpy(version, buffer, PLEN_VERSION);
			LOG(LOG_INFO, "Connected to Parallax SSC version %s", version);
			
			memmove(buffer, buffer + PLEN_VERSION, index -= PLEN_VERSION);
			state = PS_NONE;

			break;
		}
		
		case PS_NONE:
		{
			LOG(LOG_WARN, "Unexpected data received from Parallax SSC %s", serial_port);
			index = 0;
		}
	}
	
	return true;
}

void psc_update(void * object)
{
	if (psc_fd == -1)
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
	if (pwm0 != NULL)	psc_writeio(0, *pwm0);		else	psc_disableio(0);
	if (pwm1 != NULL)	psc_writeio(1, *pwm1);		else	psc_disableio(1);
	if (pwm2 != NULL)	psc_writeio(2, *pwm2);		else	psc_disableio(2);
	if (pwm3 != NULL)	psc_writeio(3, *pwm3);		else	psc_disableio(3);
	if (pwm4 != NULL)	psc_writeio(4, *pwm4);		else	psc_disableio(4);
	if (pwm5 != NULL)	psc_writeio(5, *pwm5);		else	psc_disableio(5);
	if (pwm6 != NULL)	psc_writeio(6, *pwm6);		else	psc_disableio(6);
	if (pwm7 != NULL)	psc_writeio(7, *pwm7);		else	psc_disableio(7);
	if (pwm8 != NULL)	psc_writeio(8, *pwm8);		else	psc_disableio(8);
	if (pwm9 != NULL)	psc_writeio(9, *pwm9);		else	psc_disableio(9);
	if (pwm10 != NULL)	psc_writeio(10, *pwm10);	else	psc_disableio(10);
	if (pwm11 != NULL)	psc_writeio(11, *pwm11);	else	psc_disableio(11);
	if (pwm12 != NULL)	psc_writeio(12, *pwm12);	else	psc_disableio(12);
	if (pwm13 != NULL)	psc_writeio(13, *pwm13);	else	psc_disableio(13);
	if (pwm14 != NULL)	psc_writeio(14, *pwm14);	else	psc_disableio(14);
	if (pwm15 != NULL)	psc_writeio(15, *pwm15);	else	psc_disableio(15);
}

bool psc_init()
{
	psc_fd = serial_open(serial_port, B2400);
	if (psc_fd < 0)
	{
		LOG(LOG_ERR, "Could not open Parallax SSC serial port %s", serial_port);
		return false;
	}

	// Write the set-baud command and wait for a bit
	write(psc_fd, PCMD_SETBAUD, PCMD_LENGTH);
	usleep(50000);

	// Change the serial port speed
	serial_setattr(psc_fd, B38400);

	// Register the FD and send a version request
	mainloop_addwatch(NULL, psc_fd, FD_READ, psc_newdata, NULL);
	psc_disableall();
	psc_write(PS_VERSION, PCMD_VERSION);

	return true;
}

void psc_destroy()
{
	if (psc_fd != -1)
	{
		psc_disableall();
		serial_flush(psc_fd);

		close(psc_fd);
	}
}


// TODO - make module realtime (not edge-send)
module_name("Parallax SSC Controller");
module_version(1,0,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Communicates to the Propeller Servo Controller USB");
module_oninitialize(psc_init);
module_ondestroy(psc_destroy);

config_param(serial_port,	's',		"The serial port to connect to (eg. /dev/ttyUSB0)");
config_param(servo_ramp,	'i',		"The ramp speed of the servos");

block_input(	static, 	pwm0, 		'i', 	"Channel 0 PWM");
block_input(	static, 	pwm1, 		'i', 	"Channel 1 PWM");
block_input(	static, 	pwm2, 		'i', 	"Channel 2 PWM");
block_input(	static, 	pwm3, 		'i', 	"Channel 3 PWM");
block_input(	static, 	pwm4, 		'i', 	"Channel 4 PWM");
block_input(	static, 	pwm5, 		'i', 	"Channel 5 PWM");
block_input(	static, 	pwm6, 		'i', 	"Channel 6 PWM");
block_input(	static, 	pwm7, 		'i', 	"Channel 7 PWM");
block_input(	static, 	pwm8, 		'i', 	"Channel 8 PWM");
block_input(	static, 	pwm9, 		'i', 	"Channel 9 PWM");
block_input(	static, 	pwm10, 		'i', 	"Channel 10 PWM");
block_input(	static, 	pwm11, 		'i', 	"Channel 11 PWM");
block_input(	static, 	pwm12, 		'i', 	"Channel 12 PWM");
block_input(	static, 	pwm13, 		'i', 	"Channel 13 PWM");
block_input(	static, 	pwm14, 		'i', 	"Channel 14 PWM");
block_input(	static, 	pwm15, 		'i', 	"Channel 15 PWM");
block_onupdate(	static, 	psc_update);
