SRCS		= libmax.c syscallcache.c serialize.c message.c
TARGET		= libmax.so
PACKAGES	= 
INCLUDES	= -I. -I.. -Iinclude/ -I../aul/include -I../modules/console -I../modules/console/include
DEFINES		= -DLIBMAX -D_GNU_SOURCE
CFLAGS		= -pipe -ggdb3 -Wall -std=gnu99 -fpic $(shell pkg-config --cflags $(PACKAGES) 2>/dev/null)
LIBS		= $(shell pkg-config --libs $(PACKAGES) 2>/dev/null) -laul -lrt
LFLAGS		= -shared -L../aul

OBJS		= $(SRCS:.c=.o)

.PHONY: all test install clean rebuild docs

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)
	$(MAKE) -C lua all

test: test.o
	$(CC) -o test test.o -L. -lmax -L../aul -laul

install:
	mkdir -p /usr/include/max
	cp -f $(TARGET) /usr/lib
	cp -f include/*.h /usr/include/max
	ldconfig
	$(MAKE) -C lua install

clean:
	rm -rf $(OBJS) $(TARGET) test test.o docs
	$(MAKE) -C lua clean
	$(MAKE) -C purejava clean

rebuild: clean all

docs:
	mkdir -p docs
	$(MAKE) -C purejava docs
	cp -r purejava/docs docs/purejava

%.o: %.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $*.c -o $*.o

%.o: ../%.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) ../$*.c -o $*.o

%.o: ../modules/console/%.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) ../modules/console/$*.c -o $*.o

