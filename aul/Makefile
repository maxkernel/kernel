SRCS		= error.c log.c net.c serial.c string.c mainloop.c hashtable.c queue.c
OBJS		= $(SRCS:.c=.o)

INCLUDES	= -Iinclude
DEFINES		= -D_GNU_SOURCE
CFLAGS		= -pipe -ggdb3 -Wall -fpic
LIBS		= -lrt -lz
LFLAGS		= -shared

TARGET		= libaul.so

.PHONY: install clean rebuild depend

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

install:
	cp -f $(TARGET) /usr/lib
	cp -rf include/aul /usr/include/aul
	ldconfig

clean:
	rm -f $(OBJS) $(TARGET)

rebuild: clean all

depend: $(SRCS)
	makedepend $(DEFINES) $(INCLUDES) $^

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@


# DO NOT DELETE THIS LINE -- make depend needs it

error.o: /usr/include/stdlib.h /usr/include/features.h
error.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
error.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
error.o: /usr/include/gnu/stubs-64.h /usr/include/bits/waitflags.h
error.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
error.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
error.o: /usr/include/xlocale.h /usr/include/sys/types.h
error.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
error.o: /usr/include/time.h /usr/include/sys/select.h
error.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
error.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
error.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
error.o: /usr/include/string.h include/aul/error.h include/aul/common.h
error.o: /usr/include/inttypes.h /usr/include/stdint.h
error.o: /usr/include/bits/wchar.h include/aul/string.h
log.o: /usr/include/stdio.h /usr/include/features.h
log.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
log.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
log.o: /usr/include/gnu/stubs-64.h /usr/include/bits/types.h
log.o: /usr/include/bits/typesizes.h /usr/include/libio.h
log.o: /usr/include/_G_config.h /usr/include/wchar.h
log.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
log.o: /usr/include/stdlib.h /usr/include/bits/waitflags.h
log.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
log.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
log.o: /usr/include/xlocale.h /usr/include/sys/types.h /usr/include/time.h
log.o: /usr/include/sys/select.h /usr/include/bits/select.h
log.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
log.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
log.o: /usr/include/alloca.h /usr/include/errno.h /usr/include/bits/errno.h
log.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
log.o: /usr/include/asm-generic/errno.h /usr/include/asm-generic/errno-base.h
log.o: /usr/include/inttypes.h /usr/include/stdint.h
log.o: /usr/include/bits/wchar.h /usr/include/fcntl.h
log.o: /usr/include/bits/fcntl.h /usr/include/bits/uio.h
log.o: /usr/include/sys/stat.h /usr/include/bits/stat.h
log.o: /usr/include/sys/time.h /usr/include/zlib.h /usr/include/zconf.h
log.o: /usr/include/zlibdefs.h /usr/include/unistd.h
log.o: /usr/include/bits/posix_opt.h /usr/include/bits/environments.h
log.o: /usr/include/bits/confname.h /usr/include/getopt.h
log.o: include/aul/string.h /usr/include/string.h include/aul/common.h
log.o: include/aul/log.h include/aul/error.h
net.o: /usr/include/sys/socket.h /usr/include/features.h
net.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
net.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
net.o: /usr/include/gnu/stubs-64.h /usr/include/sys/uio.h
net.o: /usr/include/sys/types.h /usr/include/bits/types.h
net.o: /usr/include/bits/typesizes.h /usr/include/time.h
net.o: /usr/include/endian.h /usr/include/bits/endian.h
net.o: /usr/include/bits/byteswap.h /usr/include/sys/select.h
net.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
net.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
net.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/uio.h
net.o: /usr/include/bits/socket.h /usr/include/bits/sockaddr.h
net.o: /usr/include/asm/socket.h /usr/include/asm-generic/socket.h
net.o: /usr/include/asm/sockios.h /usr/include/asm-generic/sockios.h
net.o: /usr/include/sys/un.h /usr/include/string.h /usr/include/xlocale.h
net.o: /usr/include/netinet/in.h /usr/include/stdint.h
net.o: /usr/include/bits/wchar.h /usr/include/bits/in.h /usr/include/unistd.h
net.o: /usr/include/bits/posix_opt.h /usr/include/bits/environments.h
net.o: /usr/include/bits/confname.h /usr/include/getopt.h
net.o: /usr/include/fcntl.h /usr/include/bits/fcntl.h /usr/include/sys/stat.h
net.o: /usr/include/bits/stat.h /usr/include/errno.h
net.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
net.o: /usr/include/asm/errno.h /usr/include/asm-generic/errno.h
net.o: /usr/include/asm-generic/errno-base.h include/aul/common.h
net.o: /usr/include/stdlib.h /usr/include/bits/waitflags.h
net.o: /usr/include/bits/waitstatus.h /usr/include/alloca.h
net.o: /usr/include/inttypes.h include/aul/log.h include/aul/error.h
net.o: include/aul/string.h include/aul/net.h
serial.o: /usr/include/fcntl.h /usr/include/features.h
serial.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
serial.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
serial.o: /usr/include/gnu/stubs-64.h /usr/include/bits/fcntl.h
serial.o: /usr/include/sys/types.h /usr/include/bits/types.h
serial.o: /usr/include/bits/typesizes.h /usr/include/time.h
serial.o: /usr/include/endian.h /usr/include/bits/endian.h
serial.o: /usr/include/bits/byteswap.h /usr/include/sys/select.h
serial.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
serial.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
serial.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/uio.h
serial.o: /usr/include/sys/stat.h /usr/include/bits/stat.h
serial.o: include/aul/common.h /usr/include/stdlib.h
serial.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
serial.o: /usr/include/xlocale.h /usr/include/alloca.h
serial.o: /usr/include/inttypes.h /usr/include/stdint.h
serial.o: /usr/include/bits/wchar.h /usr/include/string.h include/aul/log.h
serial.o: include/aul/error.h include/aul/string.h include/aul/serial.h
serial.o: /usr/include/termios.h /usr/include/bits/termios.h
serial.o: /usr/include/sys/ttydefaults.h
string.o: /usr/include/stdio.h /usr/include/features.h
string.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
string.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
string.o: /usr/include/gnu/stubs-64.h /usr/include/bits/types.h
string.o: /usr/include/bits/typesizes.h /usr/include/libio.h
string.o: /usr/include/_G_config.h /usr/include/wchar.h
string.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
string.o: /usr/include/stdlib.h /usr/include/bits/waitflags.h
string.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
string.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
string.o: /usr/include/xlocale.h /usr/include/sys/types.h /usr/include/time.h
string.o: /usr/include/sys/select.h /usr/include/bits/select.h
string.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
string.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
string.o: /usr/include/alloca.h include/aul/common.h /usr/include/inttypes.h
string.o: /usr/include/stdint.h /usr/include/bits/wchar.h
string.o: /usr/include/string.h include/aul/string.h
mainloop.o: /usr/include/stdlib.h /usr/include/features.h
mainloop.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
mainloop.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
mainloop.o: /usr/include/gnu/stubs-64.h /usr/include/bits/waitflags.h
mainloop.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
mainloop.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
mainloop.o: /usr/include/xlocale.h /usr/include/sys/types.h
mainloop.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
mainloop.o: /usr/include/time.h /usr/include/sys/select.h
mainloop.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
mainloop.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
mainloop.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
mainloop.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
mainloop.o: /usr/include/bits/environments.h /usr/include/bits/confname.h
mainloop.o: /usr/include/getopt.h /usr/include/string.h /usr/include/errno.h
mainloop.o: /usr/include/bits/errno.h /usr/include/linux/errno.h
mainloop.o: /usr/include/asm/errno.h /usr/include/asm-generic/errno.h
mainloop.o: /usr/include/asm-generic/errno-base.h /usr/include/sys/timerfd.h
mainloop.o: include/aul/log.h include/aul/common.h /usr/include/inttypes.h
mainloop.o: /usr/include/stdint.h /usr/include/bits/wchar.h
mainloop.o: include/aul/error.h include/aul/string.h include/aul/mainloop.h
mainloop.o: include/aul/mutex.h /usr/include/pthread.h /usr/include/sched.h
mainloop.o: /usr/include/bits/sched.h /usr/include/signal.h
mainloop.o: /usr/include/bits/setjmp.h include/aul/contrib/list.h
hashtable.o: /usr/include/string.h /usr/include/features.h
hashtable.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
hashtable.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
hashtable.o: /usr/include/gnu/stubs-64.h /usr/include/xlocale.h
hashtable.o: include/aul/common.h /usr/include/stdlib.h
hashtable.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
hashtable.o: /usr/include/endian.h /usr/include/bits/endian.h
hashtable.o: /usr/include/bits/byteswap.h /usr/include/sys/types.h
hashtable.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
hashtable.o: /usr/include/time.h /usr/include/sys/select.h
hashtable.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
hashtable.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
hashtable.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
hashtable.o: /usr/include/inttypes.h /usr/include/stdint.h
hashtable.o: /usr/include/bits/wchar.h include/aul/log.h include/aul/error.h
hashtable.o: include/aul/string.h include/aul/hashtable.h
hashtable.o: include/aul/contrib/list.h
queue.o: include/aul/queue.h include/aul/common.h /usr/include/stdlib.h
queue.o: /usr/include/features.h /usr/include/bits/predefs.h
queue.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
queue.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-64.h
queue.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
queue.o: /usr/include/endian.h /usr/include/bits/endian.h
queue.o: /usr/include/bits/byteswap.h /usr/include/xlocale.h
queue.o: /usr/include/sys/types.h /usr/include/bits/types.h
queue.o: /usr/include/bits/typesizes.h /usr/include/time.h
queue.o: /usr/include/sys/select.h /usr/include/bits/select.h
queue.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
queue.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
queue.o: /usr/include/alloca.h /usr/include/inttypes.h /usr/include/stdint.h
queue.o: /usr/include/bits/wchar.h /usr/include/string.h
