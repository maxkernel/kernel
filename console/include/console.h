#ifndef __CONSOLE_H
#define __CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

// If you change these, you must also change the corresponding defines in libmax

#define CONSOLE_BUFFERMAX		512
#define CONSOLE_BUFFERS			5

#define CONSOLE_SOCKFILE		"/var/local/maxkern.sock"
#define CONSOLE_TCPPORT			48000

#ifdef __cplusplus
}
#endif
#endif
