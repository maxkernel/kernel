SRCS		= libmax.c serialize.c message.c
TARGET		= libmax.so
PACKAGES	= 
INCLUDES	= -I. -I.. -Iinclude/ -I../aul/include -I../console -I../console/include
DEFINES		= -DLIBMAX -D_GNU_SOURCE
CFLAGS		= -pipe -ggdb3 -Wall -fpic $(shell pkg-config --cflags $(PACKAGES) 2>/dev/null)
LIBS		= $(shell pkg-config --libs $(PACKAGES) 2>/dev/null) -laul -lrt
LFLAGS		= -shared -L../aul


OBJS		= $(SRCS:.c=.o)

.PHONY: test install clean rebuild depend

all: $(TARGET) test

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)
	#$(MAKE) -C lua all

test: test.o
	$(CC) -o test test.o -L. -lmax

install:
	cp -f libmax.so /usr/lib
	ldconfig
	$(MAKE) -C lua install

clean:
	rm -f $(OBJS) $(TARGET)
	rm -f test test.o
	$(MAKE) -C lua clean

rebuild: clean all

depend: $(SRCS) test.c
	makedepend $(DEFINES) $(INCLUDES) -I.. -I../console $^ 2>/dev/null

%.o: %.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $*.c -o $*.o

%.o: ../%.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) ../$*.c -o $*.o

%.o: ../console/%.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) ../console/$*.c -o $*.o

# DO NOT DELETE
