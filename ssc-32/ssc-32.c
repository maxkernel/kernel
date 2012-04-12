#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <aul/serial.h>

#include <kernel.h>


static int ssc_fd = -1;
char * serial_port = "/dev/ttyUSB0";


static void ssc32_writeio(int port, int pwm)
{
	char buf[32];
	int len = snprintf(buf, sizeof(buf), "#%dP%d\r", port, pwm);
	if (len > sizeof(buf))
	{
		LOG1(LOG_ERR, "Could not write to one or more servos, internal buffer is too small!");
		return;
	}
	
	ssize_t bytes = write(ssc_fd, buf, len);
	if (bytes != len)
	{
		LOG1(LOG_ERR, "Could not write all bytes to SSC-32 %s", serial_port);
	}
}

void ssc32_update(void * obj)
{
	if (ssc_fd == -1)
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
	if (pwm0 != NULL)	ssc32_writeio(0, *pwm0);
	if (pwm1 != NULL)	ssc32_writeio(1, *pwm1);
	if (pwm2 != NULL)	ssc32_writeio(2, *pwm2);
	if (pwm3 != NULL)	ssc32_writeio(3, *pwm3);
	if (pwm4 != NULL)	ssc32_writeio(4, *pwm4);
	if (pwm5 != NULL)	ssc32_writeio(5, *pwm5);
	if (pwm6 != NULL)	ssc32_writeio(6, *pwm6);
	if (pwm7 != NULL)	ssc32_writeio(7, *pwm7);
	if (pwm8 != NULL)	ssc32_writeio(8, *pwm8);
	if (pwm9 != NULL)	ssc32_writeio(9, *pwm9);
	if (pwm10 != NULL)	ssc32_writeio(10, *pwm10);
	if (pwm11 != NULL)	ssc32_writeio(11, *pwm11);
	if (pwm12 != NULL)	ssc32_writeio(12, *pwm12);
	if (pwm13 != NULL)	ssc32_writeio(13, *pwm13);
	if (pwm14 != NULL)	ssc32_writeio(14, *pwm14);
	if (pwm15 != NULL)	ssc32_writeio(15, *pwm15);
	if (pwm16 != NULL)	ssc32_writeio(16, *pwm16);
	if (pwm17 != NULL)	ssc32_writeio(17, *pwm17);
	if (pwm18 != NULL)	ssc32_writeio(18, *pwm18);
	if (pwm19 != NULL)	ssc32_writeio(19, *pwm19);
	if (pwm20 != NULL)	ssc32_writeio(20, *pwm20);
	if (pwm21 != NULL)	ssc32_writeio(21, *pwm21);
	if (pwm22 != NULL)	ssc32_writeio(22, *pwm22);
	if (pwm23 != NULL)	ssc32_writeio(23, *pwm23);
	if (pwm24 != NULL)	ssc32_writeio(24, *pwm24);
	if (pwm25 != NULL)	ssc32_writeio(25, *pwm25);
	if (pwm26 != NULL)	ssc32_writeio(26, *pwm26);
	if (pwm27 != NULL)	ssc32_writeio(27, *pwm27);
	if (pwm28 != NULL)	ssc32_writeio(28, *pwm28);
	if (pwm29 != NULL)	ssc32_writeio(29, *pwm29);
	if (pwm30 != NULL)	ssc32_writeio(30, *pwm30);
	if (pwm31 != NULL)	ssc32_writeio(31, *pwm31);
}

bool ssc32_init()
{
	ssc_fd = serial_open(serial_port, B115200);
	if (ssc_fd == -1)
	{
		LOG(LOG_ERR, "Could not open ssc32 on serial port %s", serial_port);
		return false;
	}

	return true;
}

void ssc32_destroy()
{
	if (ssc_fd != -1)
	{
		close(ssc_fd);
		ssc_fd = -1;
	}
}


module_name("SSC-32 Controller");
module_version(1,0,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Connects to the SSC-32 Servo Controller");
module_oninitialize(ssc32_init);
module_ondestroy(ssc32_destroy);

module_config(	serial_port, 	's', 	"The serial port to connect to (eg. /dev/ttyUSB0)");

block_input(	static, 	pwm0, 		'i', 	"Channel 0 PWM");
block_input(	static, 	pwm1, 		'i', 	"Channel 1 PWM");
block_input(	static, 	pwm2, 		'i', 	"Channel 2 PWM");
block_input(	static, 	pwm3, 		'i', 	"Channel 3 PWM");
block_input(	static, 	pwm4, 		'i', 	"Channel 4 PWM");
block_input(	static, 	pwm5, 		'i', 	"Channel 5 PWM");
block_input(	static, 	pwm6, 		'i', 	"Channel 6 PWM");
block_input(	static, 	pwm7, 		'i', 	"Channel 7 PWM");
block_input(	static, 	pwm8, 		'i', 	"Channel 8 PWM");
block_input(	static, 	pwm9, 		'i', 	"Channel 9 PWM");
block_input(	static, 	pwm10, 		'i', 	"Channel 10 PWM");
block_input(	static, 	pwm11, 		'i', 	"Channel 11 PWM");
block_input(	static, 	pwm12, 		'i', 	"Channel 12 PWM");
block_input(	static, 	pwm13, 		'i', 	"Channel 13 PWM");
block_input(	static, 	pwm14, 		'i', 	"Channel 14 PWM");
block_input(	static, 	pwm15, 		'i', 	"Channel 15 PWM");
block_input(	static, 	pwm16, 		'i', 	"Channel 16 PWM");
block_input(	static, 	pwm17, 		'i', 	"Channel 17 PWM");
block_input(	static, 	pwm18, 		'i', 	"Channel 18 PWM");
block_input(	static, 	pwm19, 		'i', 	"Channel 19 PWM");
block_input(	static, 	pwm20, 		'i', 	"Channel 20 PWM");
block_input(	static, 	pwm21, 		'i', 	"Channel 21 PWM");
block_input(	static, 	pwm22, 		'i', 	"Channel 22 PWM");
block_input(	static, 	pwm23, 		'i', 	"Channel 23 PWM");
block_input(	static, 	pwm24, 		'i', 	"Channel 24 PWM");
block_input(	static, 	pwm25, 		'i', 	"Channel 25 PWM");
block_input(	static, 	pwm26, 		'i', 	"Channel 26 PWM");
block_input(	static, 	pwm27, 		'i', 	"Channel 27 PWM");
block_input(	static, 	pwm28, 		'i', 	"Channel 28 PWM");
block_input(	static, 	pwm29, 		'i', 	"Channel 29 PWM");
block_input(	static, 	pwm30, 		'i', 	"Channel 30 PWM");
block_input(	static, 	pwm31, 		'i', 	"Channel 31 PWM");
block_onupdate(	static, 	ssc32_update);
