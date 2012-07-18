#include <unistd.h>

#include <aul/serial.h>

#include <kernel.h>


static uint8_t power_default[] = { 4, 4, 4, 4 };
static uint8_t power[][4] = {
	{ 3, 3, 3, 3 },
	{ 3, 4, 3, 4 },
	{ 4, 4, 4, 4 },
	{ 4, 5, 4, 5 },
	{ 5, 5, 5, 5 },
};

typedef struct
{
	int fd;

	const uint8_t * train;
	size_t index;
} serialpwm_t;

static void * serialpwm_new(const char * port, int baud)
{
	LOG(LOG_INFO, "New serialpwm on port %s with baud %d", port, baud);

	serialpwm_t * spwm = malloc(sizeof(serialpwm_t));
	spwm->fd = serial_open(port, serial_getspeed(baud));
	spwm->train = power_default;
	spwm->index = 0;

	/*{
		// Set O_NONBLOCK on the file descriptor
		int flags = fcntl(*fd, F_GETFL, 0);
		if (fcntl(*fd, F_SETFL, flags | O_NONBLOCK) < 0)
		{
			LOG(LOG_WARN, "Could not set non-blocking option on serial port: %s", strerror(errno));
		}
	}*/

	return spwm;
}

static void serialpwm_update(void * object)
{
	serialpwm_t * spwm = object;
	if (spwm->index >= 4)
	{
		spwm->index = 0;
		const int * pwm = input(pwm);
		if (pwm != NULL)
		{
			spwm->train = power[*pwm];
		}
	}

	uint8_t value = ~((1 << spwm->train[spwm->index++]) - 1);
	write(spwm->fd, &value, sizeof(uint8_t));
}

static void serialpwm_destroy(void * object)
{
	serialpwm_t * spwm = object;
	close(spwm->fd);
}


module_name("Serial PWM thingy");
module_version(0, 1, 0);

define_block(serialpwm, "Serial pwm", serialpwm_new, "si", "Description");
block_onupdate(serialpwm, serialpwm_update);
block_ondestroy(serialpwm, serialpwm_destroy);
block_input(serialpwm, pwm, T_INTEGER, "Input pwm (1-7)");
