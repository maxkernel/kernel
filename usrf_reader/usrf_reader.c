#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/io.h>

#include <aul/serial.h>
#include <aul/mutex.h>
#include <aul/mainloop.h>

#include <kernel.h>

#define USRF_BUFSIZE	3
#define USRF_FRAMING	0xa5

typedef struct
{
	fdwatcher_t watcher;
	mutex_t lock;
	int range;

	uint8_t buffer[USRF_BUFSIZE];
	size_t index;
} usrf_t;

static bool usrf_newdata(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	labels(end);

	usrf_t * usrf = userdata;

	int bytes = read(fd, &usrf->buffer[usrf->index], USRF_BUFSIZE - usrf->index);

	if (bytes < 0)
	{
		LOG(LOG_ERR, "USRF read failed: %s", strerror(errno));
		return false;
	}

	usrf->index += bytes;
	//LOG(LOG_INFO, "READ %d bytes %zu{ %u, %u, %u }", bytes, usrf->index, usrf->buffer[0], usrf->buffer[1], usrf->buffer[2]);

	if (usrf->index == 3)
	{
		// Read three bytes, parse
		if (usrf->buffer[0] != USRF_FRAMING)
		{
			LOG(LOG_WARN, "USRF Bad Framing byte: %d", usrf->buffer[0]);

			// Bad framing!
			usrf->buffer[0] = usrf->buffer[1];
			usrf->buffer[1] = usrf->buffer[2];
			usrf->index = 2;

			goto end;
		}

		// Read out range
		mutex_lock(&usrf->lock);
		{
			usrf->range = (uint16_t)((usrf->buffer[1] & 0xff) << 8 | usrf->buffer[2]);
		}
		mutex_unlock(&usrf->lock);

		// Reset the buffer
		usrf->index = 0;
	}

end:
	return true;
}

static void * usrf_new(const char * port)
{
	LOG(LOG_INFO, "Opening USRF on port %s with baud 115200", port);

	int fd = serial_open(port, B115200);
	if (fd < 0)
	{
		LOG(LOG_ERR, "Could not open USRF!");
		return NULL;
	}

	usrf_t * usrf = malloc(sizeof(usrf_t));
	memset(usrf, 0, sizeof(usrf_t));
	watcher_newfd(&usrf->watcher, fd, FD_READ, usrf_newdata, usrf);
	mutex_init(&usrf->lock, M_RECURSIVE);
	usrf->range = 0;
	usrf->index = 0;

	{
		exception_t * e = NULL;
		if (!mainloop_addwatcher(kernel_mainloop(), &usrf->watcher, &e) || exception_check(&e))
		{
			LOG(LOG_ERR, "Could not add USRF to mainloop: %s", exception_message(e));
			exception_free(e);
		}
	}

	// Reset the arduino
	{
		/*
		int status = 0;
		ioctl(fd, TIOCMGET, &status);

		int newstatus = status | TIOCM_DTR;
		ioctl(fd, TIOCMSET, &newstatus);

		usleep(500);
		ioctl(fd, TIOCMSET, &status);
		*/
		/*
		//load status
		int status;
		ioctl(fd, TIOCMGET, &status);

		//enable DTR
		status |= TIOCM_DTR;
		ioctl(fd, TIOCMSET, &status);

		sleep(2);

		//disable DTR
		status &= ~TIOCM_DTR;
		ioctl(fd, TIOCMSET, &status);

		*/
	}


	/*{
		// Set O_NONBLOCK on the file descriptor
		int flags = fcntl(*fd, F_GETFL, 0);
		if (fcntl(*fd, F_SETFL, flags | O_NONBLOCK) < 0)
		{
			LOG(LOG_WARN, "Could not set non-blocking option on serial port: %s", strerror(errno));
		}
	}*/

	return usrf;
}

static void usrf_update(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	usrf_t * usrf = object;
	mutex_lock(&usrf->lock);
	{
		output(range, &usrf->range);
	}
	mutex_unlock(&usrf->lock);
}

static void usrf_destroy(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	usrf_t * usrf = object;

	close(watcher_fd(&usrf->watcher));
	free(usrf);
}


module_name("Ultra Sonic Range Finder - Arduino");
module_version(0, 1, 0);

define_block(srf08, "SRF08 Reader ", usrf_new, "s", "(1) Serial Port [eg /dev/ttyUSB0]");
block_onupdate(srf08, usrf_update);
block_ondestroy(srf08, usrf_destroy);
block_output(srf08, range, T_INTEGER, "Output range (microsecond round-trip-time)");

