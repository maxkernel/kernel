#include <fcntl.h>
#include <string.h>

#include <kernel.h>
#include "serial.h"

MOD_VERSION("1.0");
MOD_AUTHOR("Andrew Klofas <aklofas@gmail.com>");
MOD_DESCRIPTION("Helper module for simplified IO of serial ports");

int serial_open_bin(const char * port, speed_t speed, GIOFunc input_func)
{
	struct termios tp;
	memset(&tp, 0, sizeof(struct termios));

	int fd = open(port, O_RDWR, O_NOCTTY);
	if (fd == -1)
	{
		LOG(LOG_ERR, "Could not open serial port %s", port);
		return -1;
	}
	
	tcgetattr(fd, &tp);
	tp.c_cflag = CS8|CLOCAL|CREAD;
	tp.c_oflag = 0;
	tp.c_iflag = IGNBRK|IGNPAR;
	tp.c_lflag = 0;
	cfsetispeed(&tp, speed);
	cfsetospeed(&tp, speed);

	tcflush(fd, TCIOFLUSH);
	if (tcsetattr(fd, TCSANOW, &tp) < 0) {
		LOG(LOG_ERR, "Could not set serial port attributes for %s", port);
		return -1;
	}

	if (input_func != NULL)
	{
		GIOChannel * io = g_io_channel_unix_new(fd);
		g_io_channel_set_encoding(io, NULL, NULL);
		g_io_add_watch(io, G_IO_IN, input_func, NULL);
	}

	return fd;
}
