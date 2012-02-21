SRCS		= maxlua.c
TARGET		= max.so
PACKAGES	= lua5.1
INCLUDES	= -I. -I../.. -I../include -I../../aul/include
DEFINES		= -DLIBMAX -DLUA_LIB -D_GNU_SOURCE
CFLAGS		= -pipe -ggdb3 -Wall -std=gnu99 -fpic -shared $(shell pkg-config --cflags $(PACKAGES) 2>/dev/null)
LIBS		= $(shell pkg-config --libs $(PACKAGES) 2>/dev/null) -lmax
LFLAGS		= -L..

OBJS		= $(SRCS:.c=.o)


.PHONY: install clean rebuild depend

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

install:
	mkdir -p /usr/lib/lua/5.1
	cp -f $(TARGET) /usr/lib/lua/5.1/

clean:
	rm -f $(OBJS) $(TARGET)

rebuild: clean all

%.o: %.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $*.c -o $*.o


