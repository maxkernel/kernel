SRCS		= testmax.c
TARGET		= max-test
PACKAGES	= 
INCLUDES	= -I. -Iinclude/ -I../aul/include
DEFINES		= -DTESTMAX -D_GNU_SOURCE
CFLAGS		= -pipe -ggdb3 -Wall $(shell pkg-config --cflags $(PACKAGES) 2>/dev/null)
LIBS		= $(shell pkg-config --libs $(PACKAGES) 2>/dev/null) -laul -lrt
LFLAGS		= -shared -L../aul

OBJS		= $(SRCS:.c=.o)

.PHONY: all install clean rebuild depend

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

install:
	# Stub for now

clean:
	rm -f $(OBJS) $(TARGET)

rebuild: clean all

depend:
	makedepend $(DEFINES) $(INCLUDES) $^ 2>/dev/null

%.o: %.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $*.c -o $*.o

# DO NOT DELETE THIS LINE -- make depend needs it
