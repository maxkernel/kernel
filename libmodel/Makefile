SRCS		= meta.c
TARGET		= libmaxmodel.so
PACKAGES	= 
INCLUDES	= -I. -I.. -Iinclude/ -I../aul/include
DEFINES		= -DLIBMAXMODEL -DUSE_BFD
CFLAGS		= -pipe -ggdb3 -Wall -std=gnu99 -fpic $(shell pkg-config --cflags $(PACKAGES) 2>/dev/null)
LIBS		= $(shell pkg-config --libs $(PACKAGES) 2>/dev/null) -laul -lbfd
LFLAGS		= -shared -L../aul

OBJS		= $(SRCS:.c=.o)

.PHONY: all test install clean rebuild

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

test: test.o
	$(CC) -o test test.o -L. -lmaxmodel -L../aul -laul
	rm test.o

install:
	cp -f $(TARGET) /usr/lib
	cp -f include/*.h /usr/include
	ldconfig

clean:
	rm -f $(OBJS) $(TARGET)
	rm -f test test.o

rebuild: clean all

%.o: %.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $*.c -o $*.o

%.o: ../%.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) ../$*.c -o $*.o

