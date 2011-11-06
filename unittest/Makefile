TEST_SERIALIZE		= test_serialize.c serialize.c buffer2.c memfs.c

SRCS		= main.c $(TEST_SERIALIZE)
OBJS		= $(SRCS:.c=.o)
TARGET		= run_unittest
LOGFILE		= unittest.log

PACKAGES	= 
DEFINES		= -D_GNU_SOURCE -DUNITTEST -DLOGFILE="\"$(LOGFILE)\""
INCLUDES	= -I.. -I../aul/include -I`gcc -print-file-name=include`
LIBS		= $(shell [ -n "$(PACKAGES)" ] && pkg-config --libs $(PACKAGES)) -laul

CFLAGS		= -pipe -ggdb3 -Wall $(shell [ -n "$(PACKAGES)" ] && pkg-config --cflags $(PACKAGES))
LFLAGS		= -L../aul

.PHONY: all clean run depend

run: all
	./run_unittest

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

clean:
	rm -f $(TARGET) $(OBJS) $(LOGFILE)

depend:
	makedepend -- $(DEFINES) $(INCLUDES) -- $(SRCS) 2>/dev/null
	rm -f Makefile.bak

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

%.o: ../%.c
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@


# DO NOT DELETE THIS LINE -- make depend needs it

main.o: /usr/include/stdio.h /usr/include/features.h
main.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
main.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
main.o: /usr/include/gnu/stubs-64.h
main.o: /usr/lib/gcc/x86_64-linux-gnu/4.4.3/include/stddef.h
main.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
main.o: /usr/include/libio.h /usr/include/_G_config.h /usr/include/wchar.h
main.o: /usr/lib/gcc/x86_64-linux-gnu/4.4.3/include/stdarg.h
main.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
main.o: /usr/include/sys/ioctl.h /usr/include/bits/ioctls.h
main.o: /usr/include/asm/ioctls.h /usr/include/asm-generic/ioctls.h
main.o: /usr/include/linux/ioctl.h /usr/include/asm/ioctl.h
main.o: /usr/include/asm-generic/ioctl.h /usr/include/bits/ioctl-types.h
main.o: /usr/include/sys/ttydefaults.h /usr/include/sys/types.h
main.o: /usr/include/time.h /usr/include/endian.h /usr/include/bits/endian.h
main.o: /usr/include/bits/byteswap.h /usr/include/sys/select.h
main.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
main.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
main.o: /usr/include/bits/pthreadtypes.h unittest.h
main.o: /usr/lib/gcc/x86_64-linux-gnu/4.4.3/include/stdbool.h
