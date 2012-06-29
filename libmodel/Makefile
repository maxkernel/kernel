SRCS		= meta.c model.c luainterpret.c
TARGET		= libmaxmodel.so
PACKAGES	= lua5.1
INCLUDES	= -I. -I.. -Iinclude/ -I../aul/include
DEFINES		= -DLIBMAXMODEL -DUSE_BFD -DUSE_DL -DUSE_LUA
CFLAGS		= -pipe -ggdb3 -Wall -std=gnu99 -fpic $(shell pkg-config --cflags $(PACKAGES) 2>/dev/null)
LIBS		= $(shell pkg-config --libs $(PACKAGES) 2>/dev/null) -laul -lbfd -ldl
LFLAGS		= -shared  -rdynamic -L../aul

OBJS		= $(SRCS:.c=.o)

.PHONY: all test install clean rebuild depend

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

test: test.o
	$(CC) -o test test.o -L. -lmaxmodel -L../aul -laul
	rm test.o

test2: test2.o
	$(CC) -o libtest2.so test2.o -L. -lmaxmodel -L../aul -laul -shared -fpic
	rm test2.o

install:
	cp -f $(TARGET) /usr/lib
	cp -rf include/maxmodel /usr/include
	ldconfig

clean:
	rm -f $(OBJS) $(TARGET)
	rm -f test libtest2.so

rebuild: clean all

depend: $(SRCS)
	makedepend $(DEFINES) $(INCLUDES) $^ 2>/dev/null

%.o: %.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) $*.c -o $*.o

%.o: ../%.c
	$(CC) -c $(CFLAGS) $(DEFINES) $(INCLUDES) ../$*.c -o $*.o

# DO NOT DELETE THIS LINE -- make depend needs it

