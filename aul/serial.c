#include <fcntl.h>

#include <aul/common.h>
#include <aul/serial.h>


int serial_open(const char * port, speed_t speed)
{
	int fd = open(port, O_RDWR, O_NOCTTY);
	if (fd == -1)
	{
		// Could not open serial port!
		return -1;
	}

	tcflush(fd, TCIOFLUSH);
	serial_setattr(fd, speed);

	return fd;
}

void serial_flush(int fd)
{
	tcflush(fd, TCIOFLUSH);
}

bool serial_setattr(int fd, speed_t speed)
{
	struct termios tp;
	memset(&tp, 0, sizeof(struct termios));
	tp.c_iflag = IGNBRK|IGNPAR;
	tp.c_oflag = 0;
	tp.c_cflag = CS8|CREAD|CLOCAL;
	tp.c_lflag = 0;

	cfsetispeed(&tp, speed);
	cfsetospeed(&tp, speed);

	if (tcsetattr(fd, TCSAFLUSH, &tp) < 0)
	{
		// Could not set serial port attributes!
		return false;
	}

	return true;
}

speed_t serial_getspeed(int baud)
{
	switch (baud)
	{
		case 2400:		return B2400;
		case 4800:		return B4800;
		case 9600:		return B9600;
		case 19200:		return B19200;
		case 38400:		return B38400;
		case 57600:		return B57600;
		case 115200:	return B115200;
		default:		return 0;
	}
}
