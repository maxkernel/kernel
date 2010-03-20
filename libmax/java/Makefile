OBJECTS		= wrapper.o
PACKAGES	= glib-2.0 
DEFINES		=

TARGET		= libmax-java.so
LIBS		= $(shell pkg-config --libs $(PACKAGES)) -L.. -lmax
CFLAGS		= -pipe -ggdb3 -Wall -I/usr/lib/jvm/java-6-sun/include -I/usr/lib/jvm/java-6-sun/include/linux -I. -I.. -I../.. -DLIBMAX $(DEFINES) -fpic $(shell pkg-config --cflags $(PACKAGES))


all: $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) -shared -Wl,-soname,$(TARGET) $(LIBS)
	cd classes && /usr/lib/jvm/java-6-sun/bin/javac org/senseta/MaxRobot.java
	/usr/lib/jvm/java-6-sun/bin/jar cf libmax.jar -C classes .
	/usr/lib/jvm/java-6-sun/bin/javac -cp libmax.jar Test.java
	rm -f $(OBJECTS) classes/org/senseta/MaxRobot.class

clean:
	rm -f $(TARGET) libmax.jar Test.class

install:
	cp -f libmax.jar /usr/lib/jvm/java-6-sun/jre/lib/ext
	cp -f libmax-java.so /usr/lib/jvm/java-6-sun/jre/lib/i386
	echo "/usr/lib/jvm/java-6-sun/jre/lib/i386/client/" >/etc/ld.so.conf.d/java.conf
	ldconfig

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o
