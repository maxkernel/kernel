#ifndef __SERIAL_H
#define __SERIAL_H

#include <glib.h>
#include <unistd.h>
#include <termios.h>

#ifdef __cplusplus
extern "C" {
#endif

int serial_open_bin(const char * port, speed_t speed, GIOFunc input_func);

#ifdef __cplusplus
}
#endif
#endif
