OBJECTS		= error.o log.o net.o string.o list.o mainloop.o
DEFINES		= -D_GNU_SOURCE

LIBS		= -lrt -lz
CFLAGS		= -pipe -ggdb3 -Wall -Iinclude $(DEFINES) -fpic

all: prepare $(OBJECTS)
	$(CC) -o libaul.so.0 $(foreach object,$(OBJECTS), obj/$(object)) -shared -Wl,-soname,libaul.so $(LIBS)
	ln -sf libaul.so.0 libaul.so
	rm -rf obj

install:
	cp -f libaul.so.0 /usr/lib
	cp -rf include/aul /usr/include/aul
	ldconfig

clean:
	rm -rf libaul.so libaul.so.0 obj

rebuild: clean all

prepare:
	mkdir -p obj

%.o: src/%.c
	$(CC) -c $(CFLAGS) src/$*.c -o obj/$*.o
	
