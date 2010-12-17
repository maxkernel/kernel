SRCS		= libmax.c serialize.c buffer.c
OBJS		= $(SRCS:.c=.o)
PACKAGES	= glib-2.0 gnet-2.0
INCLUDES	= -I. -I.. -I../console/include
DEFINES		= -DLIBMAX
CFLAGS		= -pipe -ggdb3 -Wall -fpic $(shell pkg-config --cflags $(PACKAGES))
LIBS		= $(shell pkg-config --libs $(PACKAGES))
LFLAGS		= -shared

TARGET		= libmax.so

.PHONY: install clean rebuild depend

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

install:
	

clean:
	rm -f $(OBJS) $(TARGET)

rebuild: clean all

depend: $(SRCS)
	makedepend $(DEFINES) $(INCLUDES) $^

#.c.o:
#	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

#all: $(OBJECTS) test.o
#	$(CC) -o libmax.so.0 $(OBJECTS) -shared -Wl,-soname,libmax.so $(LIBS)
#	ln -sf libmax.so.0 libmax.so
#	$(CC) -o test test.o -L. -lmax -ldl
#	rm -f $(OBJECTS) test.o
#	$(MAKE) -C java all
#	$(MAKE) -C lua all

#clean:
#	rm -f libmax.so.0 libmax.so test
#	$(MAKE) -C java clean
#	$(MAKE) -C lua clean

#install:
#	cp -f libmax.so.0 /usr/lib
#	ldconfig
#	$(MAKE) -C java install
#	$(MAKE) -C lua install

%.o: %.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $*.c -o $*.o

%.o: ../%.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) ../$*.c -o $*.o

# DO NOT DELETE THIS LINE -- make depend needs it

