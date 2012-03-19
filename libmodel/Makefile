SRCS		= meta.c model.c
TARGET		= libmaxmodel.so
PACKAGES	= 
INCLUDES	= -I. -I.. -Iinclude/ -I../aul/include
DEFINES		= -DLIBMAXMODEL -DUSE_BFD -DUSE_DL
CFLAGS		= -pipe -ggdb3 -Wall -std=gnu99 -fpic $(shell pkg-config --cflags $(PACKAGES) 2>/dev/null)
LIBS		= $(shell pkg-config --libs $(PACKAGES) 2>/dev/null) -laul -lbfd -ldl
LFLAGS		= -shared  -rdynamic -L../aul

OBJS		= $(SRCS:.c=.o)

.PHONY: all test install clean rebuild depend

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

test: test.o
	$(CC) -o test test.o -L. -lmaxmodel -L../aul -laul
	rm test.o

test2: test2.o
	$(CC) -o libtest2.so test2.o -L. -lmaxmodel -L../aul -laul -shared -fpic
	rm test2.o

install:
	cp -f $(TARGET) /usr/lib
	cp -f include/*.h /usr/include
	ldconfig

clean:
	rm -f $(OBJS) $(TARGET)
	rm -f test test.o

rebuild: clean all

depend: $(SRCS)
	makedepend $(DEFINES) $(INCLUDES) $^ 2>/dev/null

%.o: %.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $*.c -o $*.o

%.o: ../%.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) ../$*.c -o $*.o

# DO NOT DELETE THIS LINE -- make depend needs it

meta.o: /usr/include/stdio.h /usr/include/features.h /usr/include/libio.h
meta.o: /usr/include/_G_config.h /usr/include/wchar.h /usr/include/string.h
meta.o: /usr/include/xlocale.h /usr/include/errno.h
meta.o: ../aul/include/aul/base64.h /usr/include/stdlib.h
meta.o: /usr/include/alloca.h /usr/include/stdint.h include/maxmeta.h
meta.o: ../aul/include/aul/common.h /usr/include/inttypes.h
meta.o: ../aul/include/aul/exception.h ../aul/include/aul/string.h
meta.o: ../aul/include/aul/version.h /usr/include/bfd.h
meta.o: /usr/include/ansidecl.h /usr/include/symcat.h
model.o: /usr/include/stdlib.h /usr/include/features.h /usr/include/alloca.h
model.o: /usr/include/string.h /usr/include/xlocale.h /usr/include/errno.h
model.o: include/model.h /usr/include/inttypes.h /usr/include/stdint.h
model.o: ../aul/include/aul/exception.h ../aul/include/aul/common.h
model.o: ../aul/include/aul/string.h include/maxmeta.h
model.o: ../aul/include/aul/version.h
