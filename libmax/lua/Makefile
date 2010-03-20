OBJECTS		= maxlua.o
PACKAGES	= glib-2.0 lua5.1 
DEFINES		=

TARGET		= max.so
LIBS		= $(shell pkg-config --libs $(PACKAGES)) -L.. -lmax
CFLAGS		= -pipe -ggdb3 -Wall -I. -I.. -I../.. -DLIBMAX -DLUA_LIB $(DEFINES) -fpic $(shell pkg-config --cflags $(PACKAGES))


all: $(OBJECTS)
	$(CC) -o $(TARGET) -shared -fpic $(OBJECTS) $(LIBS)
	rm -f $(OBJECTS)

clean:
	rm -f $(TARGET)

install:
	cp -f $(TARGET) /usr/lib/lua/5.1/

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o
