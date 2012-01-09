#include <unistd.h>
#include <stddef.h>
#include <string.h>

#include <aul/string.h>
#include <aul/serial.h>
#include <aul/mainloop.h>
#include <kernel.h>

// Module defines
MOD_VERSION("1.0");
MOD_AUTHOR("Andrew Klofas <andrew@maxkernel.com>");
MOD_DESCRIPTION("Communicates to the Propeller Servo Controller USB");
MOD_INIT(psc_init);
MOD_DESTROY(psc_destroy);

BLK_INPUT(STATIC, pwm0, "i");
BLK_INPUT(STATIC, pwm1, "i");
BLK_INPUT(STATIC, pwm2, "i");
BLK_INPUT(STATIC, pwm3, "i");
BLK_INPUT(STATIC, pwm4, "i");
BLK_INPUT(STATIC, pwm5, "i");
BLK_INPUT(STATIC, pwm6, "i");
BLK_INPUT(STATIC, pwm7, "i");
BLK_INPUT(STATIC, pwm8, "i");
BLK_INPUT(STATIC, pwm9, "i");
BLK_INPUT(STATIC, pwm10, "i");
BLK_INPUT(STATIC, pwm11, "i");
BLK_INPUT(STATIC, pwm12, "i");
BLK_INPUT(STATIC, pwm13, "i");
BLK_INPUT(STATIC, pwm14, "i");
BLK_INPUT(STATIC, pwm15, "i");
BLK_ONUPDATE(STATIC, psc_update);

CFG_PARAM(serial_port,		"s");
CFG_PARAM(servo_ramp,		"i");


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


static int psc_fd = -1;
static rxstate_t state = PS_NONE;
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

static void psc_writeio(int channel, const int value)
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
	if (pwm0 != NULL)	psc_writeio(0, *pwm0);
	if (pwm1 != NULL)	psc_writeio(1, *pwm1);
	if (pwm2 != NULL)	psc_writeio(2, *pwm2);
	if (pwm3 != NULL)	psc_writeio(3, *pwm3);
	if (pwm4 != NULL)	psc_writeio(4, *pwm4);
	if (pwm5 != NULL)	psc_writeio(5, *pwm5);
	if (pwm6 != NULL)	psc_writeio(6, *pwm6);
	if (pwm7 != NULL)	psc_writeio(7, *pwm7);
	if (pwm8 != NULL)	psc_writeio(8, *pwm8);
	if (pwm9 != NULL)	psc_writeio(9, *pwm9);
	if (pwm10 != NULL)	psc_writeio(10, *pwm10);
	if (pwm11 != NULL)	psc_writeio(11, *pwm11);
	if (pwm12 != NULL)	psc_writeio(12, *pwm12);
	if (pwm13 != NULL)	psc_writeio(13, *pwm13);
	if (pwm14 != NULL)	psc_writeio(14, *pwm14);
	if (pwm15 != NULL)	psc_writeio(15, *pwm15);
}

void psc_init() {
	psc_fd = serial_open(serial_port, B2400);
	if (psc_fd < 0)
	{
		LOG(LOG_ERR, "Could not open Propeller SSC serial port %s", serial_port);
		return;
	}
	
	mainloop_addwatch(NULL, psc_fd, FD_READ, psc_newdata, NULL);
	psc_write(PS_VERSION, PCMD_VERSION);
}

void psc_destroy()
{
	if (psc_fd != -1)
	{
		close(psc_fd);
	}
}

