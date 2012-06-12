#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <aul/serial.h>

#include <kernel.h>


static void ssc32_writeio(int fd, int port, int pwm)
{
	char buf[32];
	int len = snprintf(buf, sizeof(buf), "#%dP%d\r", port, pwm);
	if (len > sizeof(buf))
	{
		LOG1(LOG_ERR, "Could not write to one or more servos, internal buffer is too small!");
		return;
	}
	
	ssize_t bytes = write(fd, buf, len);
	if (bytes != len)
	{
		LOG1(LOG_ERR, "Could not write all bytes to SSC-32");
	}
}

void ssc32_update(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	// Get the file descriptor
	int fd = *(int *)object;

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
	const int * pwm16 = INPUT(pwm16);
	const int * pwm17 = INPUT(pwm17);
	const int * pwm18 = INPUT(pwm18);
	const int * pwm19 = INPUT(pwm19);
	const int * pwm20 = INPUT(pwm20);
	const int * pwm21 = INPUT(pwm21);
	const int * pwm22 = INPUT(pwm22);
	const int * pwm23 = INPUT(pwm23);
	const int * pwm24 = INPUT(pwm24);
	const int * pwm25 = INPUT(pwm25);
	const int * pwm26 = INPUT(pwm26);
	const int * pwm27 = INPUT(pwm27);
	const int * pwm28 = INPUT(pwm28);
	const int * pwm29 = INPUT(pwm29);
	const int * pwm30 = INPUT(pwm30);
	const int * pwm31 = INPUT(pwm31);
	

	// Now write it to the servos
	if (pwm0 != NULL)	ssc32_writeio(fd, 0, *pwm0);
	if (pwm1 != NULL)	ssc32_writeio(fd, 1, *pwm1);
	if (pwm2 != NULL)	ssc32_writeio(fd, 2, *pwm2);
	if (pwm3 != NULL)	ssc32_writeio(fd, 3, *pwm3);
	if (pwm4 != NULL)	ssc32_writeio(fd, 4, *pwm4);
	if (pwm5 != NULL)	ssc32_writeio(fd, 5, *pwm5);
	if (pwm6 != NULL)	ssc32_writeio(fd, 6, *pwm6);
	if (pwm7 != NULL)	ssc32_writeio(fd, 7, *pwm7);
	if (pwm8 != NULL)	ssc32_writeio(fd, 8, *pwm8);
	if (pwm9 != NULL)	ssc32_writeio(fd, 9, *pwm9);
	if (pwm10 != NULL)	ssc32_writeio(fd, 10, *pwm10);
	if (pwm11 != NULL)	ssc32_writeio(fd, 11, *pwm11);
	if (pwm12 != NULL)	ssc32_writeio(fd, 12, *pwm12);
	if (pwm13 != NULL)	ssc32_writeio(fd, 13, *pwm13);
	if (pwm14 != NULL)	ssc32_writeio(fd, 14, *pwm14);
	if (pwm15 != NULL)	ssc32_writeio(fd, 15, *pwm15);
	if (pwm16 != NULL)	ssc32_writeio(fd, 16, *pwm16);
	if (pwm17 != NULL)	ssc32_writeio(fd, 17, *pwm17);
	if (pwm18 != NULL)	ssc32_writeio(fd, 18, *pwm18);
	if (pwm19 != NULL)	ssc32_writeio(fd, 19, *pwm19);
	if (pwm20 != NULL)	ssc32_writeio(fd, 20, *pwm20);
	if (pwm21 != NULL)	ssc32_writeio(fd, 21, *pwm21);
	if (pwm22 != NULL)	ssc32_writeio(fd, 22, *pwm22);
	if (pwm23 != NULL)	ssc32_writeio(fd, 23, *pwm23);
	if (pwm24 != NULL)	ssc32_writeio(fd, 24, *pwm24);
	if (pwm25 != NULL)	ssc32_writeio(fd, 25, *pwm25);
	if (pwm26 != NULL)	ssc32_writeio(fd, 26, *pwm26);
	if (pwm27 != NULL)	ssc32_writeio(fd, 27, *pwm27);
	if (pwm28 != NULL)	ssc32_writeio(fd, 28, *pwm28);
	if (pwm29 != NULL)	ssc32_writeio(fd, 29, *pwm29);
	if (pwm30 != NULL)	ssc32_writeio(fd, 30, *pwm30);
	if (pwm31 != NULL)	ssc32_writeio(fd, 31, *pwm31);
}

void * ssc32_new(const char * serial_port)
{
	int fd = serial_open(serial_port, B115200);
	if (fd == -1)
	{
		LOG(LOG_ERR, "Could not open ssc32 on serial port %s", serial_port);
		return NULL;
	}

	int * fp = malloc(sizeof(int));
	*fp = fd;
	return fp;
}

void ssc32_destroy(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	int * fp = object;
	close(*fp);
	free(fp);
}



define_block(	ssc32, "SSC32 Controller (115200 Baud)", ssc32_new, "s", "(1) Serial port [eg. /dev/ttyUSB0]");
block_onupdate(	ssc32, 	ssc32_update);
block_ondestroy(ssc32,	ssc32_destroy);
block_input(	ssc32, 	pwm0, 		'i', 	"Channel 0 PWM");
block_input(	ssc32, 	pwm1, 		'i', 	"Channel 1 PWM");
block_input(	ssc32, 	pwm2, 		'i', 	"Channel 2 PWM");
block_input(	ssc32, 	pwm3, 		'i', 	"Channel 3 PWM");
block_input(	ssc32, 	pwm4, 		'i', 	"Channel 4 PWM");
block_input(	ssc32, 	pwm5, 		'i', 	"Channel 5 PWM");
block_input(	ssc32, 	pwm6, 		'i', 	"Channel 6 PWM");
block_input(	ssc32, 	pwm7, 		'i', 	"Channel 7 PWM");
block_input(	ssc32, 	pwm8, 		'i', 	"Channel 8 PWM");
block_input(	ssc32, 	pwm9, 		'i', 	"Channel 9 PWM");
block_input(	ssc32, 	pwm10, 		'i', 	"Channel 10 PWM");
block_input(	ssc32, 	pwm11, 		'i', 	"Channel 11 PWM");
block_input(	ssc32, 	pwm12, 		'i', 	"Channel 12 PWM");
block_input(	ssc32, 	pwm13, 		'i', 	"Channel 13 PWM");
block_input(	ssc32, 	pwm14, 		'i', 	"Channel 14 PWM");
block_input(	ssc32, 	pwm15, 		'i', 	"Channel 15 PWM");
block_input(	ssc32, 	pwm16, 		'i', 	"Channel 16 PWM");
block_input(	ssc32, 	pwm17, 		'i', 	"Channel 17 PWM");
block_input(	ssc32, 	pwm18, 		'i', 	"Channel 18 PWM");
block_input(	ssc32, 	pwm19, 		'i', 	"Channel 19 PWM");
block_input(	ssc32, 	pwm20, 		'i', 	"Channel 20 PWM");
block_input(	ssc32, 	pwm21, 		'i', 	"Channel 21 PWM");
block_input(	ssc32, 	pwm22, 		'i', 	"Channel 22 PWM");
block_input(	ssc32, 	pwm23, 		'i', 	"Channel 23 PWM");
block_input(	ssc32, 	pwm24, 		'i', 	"Channel 24 PWM");
block_input(	ssc32, 	pwm25, 		'i', 	"Channel 25 PWM");
block_input(	ssc32, 	pwm26, 		'i', 	"Channel 26 PWM");
block_input(	ssc32, 	pwm27, 		'i', 	"Channel 27 PWM");
block_input(	ssc32, 	pwm28, 		'i', 	"Channel 28 PWM");
block_input(	ssc32, 	pwm29, 		'i', 	"Channel 29 PWM");
block_input(	ssc32, 	pwm30, 		'i', 	"Channel 30 PWM");
block_input(	ssc32, 	pwm31, 		'i', 	"Channel 31 PWM");
