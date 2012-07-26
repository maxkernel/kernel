#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/io.h>

#include <aul/serial.h>
#include <aul/mutex.h>
#include <aul/mainloop.h>

#include <kernel.h>

#define FRAMING	0xa5

typedef struct
{
	fdwatcher_t watcher;
} blackbox_t;

static bool usrf_newdata(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	blackbox_t * bb = userdata;
	unused(bb);

	return true;
}

static void * bb_new(const char * port)
{
	LOG(LOG_INFO, "Opening BlackBox on port %s with baud 115200", port);

	int fd = serial_open(port, B115200);
	if (fd < 0)
	{
		LOG(LOG_ERR, "Could not open USRF!");
		return NULL;
	}

	blackbox_t * bb = malloc(sizeof(blackbox_t));
	memset(bb, 0, sizeof(blackbox_t));
	watcher_newfd(&bb->watcher, fd, FD_READ, usrf_newdata, bb);

	{
		exception_t * e = NULL;
		if (!mainloop_addwatcher(kernel_mainloop(), &bb->watcher, &e) || exception_check(&e))
		{
			LOG(LOG_ERR, "Could not add USRF to mainloop: %s", exception_message(e));
			exception_free(e);
		}
	}

	/*{
		// Set O_NONBLOCK on the file descriptor
		int flags = fcntl(*fd, F_GETFL, 0);
		if (fcntl(*fd, F_SETFL, flags | O_NONBLOCK) < 0)
		{
			LOG(LOG_WARN, "Could not set non-blocking option on serial port: %s", strerror(errno));
		}
	}*/

	return bb;
}

static void bb_update(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	blackbox_t * bb = object;
	int8_t leftcmd = 0, rightcmd = 0;

	const double * left = input(left);
	if (left != NULL)
	{
		leftcmd = (int8_t)(clamp(*left, -1.0, 1.0) * 127.0);
	}

	const double * right = input(right);
	if (right != NULL)
	{
		rightcmd = (int8_t)(clamp(*right, -1.0, 1.0) * 127.0);
	}

	uint8_t data[] = { FRAMING, leftcmd, rightcmd };
	if (write(watcher_fd(&bb->watcher), data, nelems(data)) != sizeof(data))
	{
		LOG1(LOG_ERR, "Could not write all data to blackbox rover");
	}
}

static void bb_destroy(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	blackbox_t * bb = object;

	close(watcher_fd(&bb->watcher));
	free(bb);
}


module_name("BlackBox robot controller");
module_version(0, 1, 0);

define_block(blackbox, "Robot controller ", bb_new, "s", "(1) Serial Port [eg /dev/ttyUSB0]");
block_onupdate(blackbox, bb_update);
block_ondestroy(blackbox, bb_destroy);
block_input(blackbox, left, T_DOUBLE, "Left motor speed (-1.0 to 1.0) with braking");
block_input(blackbox, right, T_DOUBLE, "Right motor speed (-1.0 to 1.0) with braking");
