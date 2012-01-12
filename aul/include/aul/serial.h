#ifndef __AUL_SERIAL_H
#define __AUL_SERIAL_H

#include <termios.h>

#ifdef __cplusplus
extern "C" {
#endif

int serial_open(const char * port, speed_t speed);
void serial_flush(int fd);
void serial_setattr(int fd, speed_t speed);
speed_t serial_getspeed(int baud);

#ifdef __cplusplus
}
#endif
#endif
