SRCS		= exception.c log.c net.c serial.c string.c mainloop.c hashtable.c iterator.c
OBJS		= $(SRCS:.c=.o)

INCLUDES	= -Iinclude
DEFINES		= -D_GNU_SOURCE
CFLAGS		= -pipe -ggdb3 -Wall -std=gnu99 -fpic
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
	makedepend $(DEFINES) $(INCLUDES) $^ 2>/dev/null

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@


# DO NOT DELETE THIS LINE -- make depend needs it

