#ifndef __CONSOLE_H
#define __CONSOLE_H

#include <kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

// If you change these, you must also change the corresponding defines in libmax

#define CONSOLE_BUFFERMAX		SYSCALL_BUFFERMAX
#define CONSOLE_HEADERSIZE		10
#define CONSOLE_BUFFERS			5

#define CONSOLE_SOCKFILE		"/var/local/maxkern.sock"
#define CONSOLE_TCPPORT			48000

#define CONSOLE_FRAMEING		0xA5A5A5A5

#ifdef __cplusplus
}
#endif
#endif
