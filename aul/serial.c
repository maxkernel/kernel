#include <unistd.h>
#include <fcntl.h>

#include <aul/common.h>
#include <aul/serial.h>

// TODO IMPORTANT - add exception_t to this function!
int serial_open(const char * port, speed_t speed)
{
	int fd = open(port, O_RDWR, O_NOCTTY);
	if (fd == -1)
	{
		// Could not open serial port!
		return -1;
	}

	serial_flush(fd);
	if (!serial_setattr(fd, speed))
	{
		close(fd);
		return -1;
	}

	//tcflush(fd, TCIOFLUSH);

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

	tcgetattr(fd, &tp);
	cfmakeraw(&tp);

	tp.c_cflag |= CREAD;
	tp.c_iflag |= IGNBRK;

	// TODO - fix this piece of shit!
#if 0
	tp.c_cflag = speed|CS8|CREAD|CLOCAL;
	tp.c_iflag = /*IGNBRK|*/IGNPAR;
	tp.c_oflag = 0;
	tp.c_lflag = 0;
#endif

	cfsetspeed(&tp, speed);

	//cfsetispeed(&tp, speed);
	//cfsetospeed(&tp, speed);

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
		case 150:		return B150;
		case 200:		return B200;
		case 300:		return B300;
		case 600:		return B600;
		case 1200:		return B1200;
		case 1800:		return B1800;
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
