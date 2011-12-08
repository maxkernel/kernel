#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <aul/serial.h>

#include <kernel.h>


MOD_VERSION("0.9");
MOD_AUTHOR("Andrew Klofas");
MOD_DESCRIPTION("Connects to the SSC-32 Servo Controller");
MOD_INIT(module_init);

CFG_PARAM(serial_port, "s");

DEF_SYSCALL(doio, "v:ii", "Puts the port (1) to the PWM position (2)");


static int ssc_fd = -1;



char * serial_port = "/dev/ttyUSB0";


void doio(int port, int pwm)
{
	char buf[100];
	sprintf(buf, "#%dP%d\r", port, pwm);
	
	//LOG(LOG_DEBUG, "DOIO: %s", buf);
	
	write(ssc_fd, buf, strlen(buf));
}

void module_init() {
	ssc_fd = serial_open(serial_port, B115200);
	
}
