#ifndef __AUL_SERIAL_H
#define __AUL_SERIAL_H

#include <termios.h>

#ifdef __cplusplus
extern "C" {
#endif

int serial_open(const char * port, speed_t speed);


#ifdef __cplusplus
}
#endif
#endif
