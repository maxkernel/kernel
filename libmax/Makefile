OBJECTS		= libmax.o serialize.o buffer.o
PACKAGES	= glib-2.0 gnet-2.0
DEFINES		= -DLIBMAX

LIBS		= $(shell pkg-config --libs $(PACKAGES))
CFLAGS		= -pipe -ggdb3 -Wall -I. -I.. -I../console/include $(DEFINES) -fpic $(shell pkg-config --cflags $(PACKAGES))


all: $(OBJECTS) test.o
	$(CC) -o libmax.so.0 $(OBJECTS) -shared -Wl,-soname,libmax.so $(LIBS)
	ln -sf libmax.so.0 libmax.so
	$(CC) -o test test.o -L. -lmax -ldl
	rm -f $(OBJECTS) test.o
	$(MAKE) -C java all
	$(MAKE) -C lua all

clean:
	rm -f libmax.so.0 libmax.so test
	$(MAKE) -C java clean
	$(MAKE) -C lua clean

install:
	cp -f libmax.so.0 /usr/lib
	ldconfig
	$(MAKE) -C java install
	$(MAKE) -C lua install

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o

%.o: ../%.c
	$(CC) -c $(CFLAGS) ../$*.c -o $*.o

