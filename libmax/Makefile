LDCONFIG	= /sbin/ldconfig
DOXYGEN		= /usr/bin/doxygen

SRCS		= libmax.c syscallcache.c serialize.c message.c
SUBDIRS		= lua purejava matlab
TARGET		= libmax.so
PACKAGES	= 
INCLUDES	= -I. -I.. -Iinclude/ -I../aul/include -I../modules/console -I../modules/console/include
DEFINES		= -DLIBMAX -D_GNU_SOURCE
CFLAGS		= -pipe -ggdb3 -Wall -std=gnu99 -fpic $(shell pkg-config --cflags $(PACKAGES) 2>/dev/null)
LIBS		= $(shell pkg-config --libs $(PACKAGES) 2>/dev/null) -laul -lrt
LFLAGS		= -shared -L../aul

OBJS		= $(SRCS:.c=.o)

.PHONY: all subdirs test install clean rebuild docs

all: $(TARGET) subdirs

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

subdirs:
	$(foreach subdir,$(SUBDIRS), $(MAKE) -C $(subdir) all &&) true

test: test.o
	$(CC) -o test test.o -L. -lmax -L../aul -laul

install:
	mkdir -p /usr/include/max
	cp -f $(TARGET) /usr/lib
	cp -f include/*.h /usr/include/max
	$(foreach subdir,$(SUBDIRS), $(MAKE) -C $(subdir) install &&) true
	$(LDCONFIG)

clean:
	rm -rf $(OBJS) $(TARGET) test test.o doxygen
	$(foreach subdir,$(SUBDIRS), $(MAKE) -C $(subdir) clean &&) true

rebuild: clean all

docs:
	mkdir -p doxygen
	$(DOXYGEN) Doxyfile
	$(foreach subdir,$(SUBDIRS), $(MAKE) -C $(subdir) docs && cp -r $(subdir)/doxygen doxygen/$(subdir) &&) true

%.o: %.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $*.c -o $*.o

%.o: ../%.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) ../$*.c -o $*.o

%.o: ../modules/console/%.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) ../modules/console/$*.c -o $*.o

