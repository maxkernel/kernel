TARGET		= dashboard
OBJECTS		= main.o
PACKAGES	= sdl

LIBS		= $(shell pkg-config --libs $(PACKAGES)) -L../../libmax -lmax
CFLAGS		= -pipe -ggdb3 -Wall -I. -I../../libmax -I../.. $(shell pkg-config --cflags $(PACKAGES))

all: $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) $(LIBS)

clean:
	rm -f $(TARGET) $(OBJECTS)

rebuild: clean all

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o

