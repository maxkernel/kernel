#include <aul/mainloop.h>
#include <aul/serial.h>

#include <kernel.h>


#define RC_START		128
#define RC_CONTROL		130
#define RC_SAFE			131
#define RC_FULL			132



typedef struct
{
	fdwatcher_t watcher;
} roomba_t;

static bool roomba_newdata(mainloop_t * loop, int fd, fdcond_t cond, void * userdata)
{

	return true;
}

static void * roomba_new(const char * serial_port, int baud)
{
	int fd = serial_open(serial_port, serial_getspeed(baud));
	if (fd < 0)
	{
		LOG(LOG_ERR, "Could not open roomba serial port %s at baud %d", serial_port, baud);
		return NULL;
	}

	roomba_t * roomba = malloc(sizeof(roomba_t));
	memset(roomba, 0, sizeof(roomba_t));
	watcher_newfd(&roomba->watcher, fd, FD_READ, roomba_newdata, roomba);

	// Add watcher to mainloop
	{
		exception_t * e = NULL;
		if (!mainloop_addwatcher(kernel_mainloop(), &roomba->watcher, &e) || exception_check(&e))
		{
			LOG(LOG_ERR, "Could not add roomba serial watcher to mainloop: %s", exception_message(e));
			exception_free(e);

			free(roomba);
			return NULL;
		}
	}

	// Enable the roomba in full mode
	{
		uint8_t data[] = { RC_START, RC_CONTROL, RC_FULL };
		for (size_t i = 0; i < nelems(data); i++)
		{
			write(fd, &data[i], sizeof(uint8_t));
			usleep(25 * MILLIS_PER_SECOND);
		}
	}

	return roomba;
}

static void roomba_update(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	roomba_t * roomba = object;

}

static void roomba_destroy(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	roomba_t * roomba = object;

	// Remove the serial watcher
	{
		exception_t * e = NULL;
		if (!mainloop_removewatcher(&roomba->watcher, NULL) || exception_check(&e))
		{
			LOG(LOG_WARN, "Could not remove roomba watcher from mainloop: %s", exception_message(e));
			exception_free(e);
		}
	}
}


module_name("iRobot Roomba SCI driver");
module_version(0,9,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Connects to the iRobot Roomba vacuum cleaner over serial");

define_block(	roomba, "Roomba vacuum", roomba_new, "si", "(1) Serial port to connect over [eg. /dev/ttyUSB0] (2) Serial port baud [default 57600]");
block_onupdate(	roomba, roomba_update);
block_ondestroy(roomba, roomba_destroy);
