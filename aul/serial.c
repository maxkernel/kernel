#include <fcntl.h>

#include <aul/common.h>
#include <aul/log.h>
#include <aul/serial.h>


int serial_open(const char * port, speed_t speed)
{
	struct termios tp;
	ZERO(tp);

	int fd = open(port, O_RDWR, O_NOCTTY);
	if (fd == -1)
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Could not open serial port %s", port);
		return -1;
	}

// TODO - fix this!!!!! These parameters are NOT correct!
	tcgetattr(fd, &tp);
//	tp.c_cflag = CS8|CLOCAL|CREAD;
//	tp.c_oflag = 0;
//	tp.c_iflag = IGNBRK|IGNPAR;
//	tp.c_lflag = 0;
	tp.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	tp.c_oflag &= ~OPOST;
	tp.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tp.c_cflag &= ~(CSIZE | PARENB);
	tp.c_cflag |= CS8;

	cfsetispeed(&tp, speed);
	cfsetospeed(&tp, speed);

	tcflush(fd, TCIOFLUSH);
	if (tcsetattr(fd, TCSANOW, &tp) < 0)
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Could not set serial port attributes for %s", port);
		return -1;
	}

	return fd;
}
