SYSCALL_OBJECTS		= syscall.o
SYSCALL_PACKAGES	= glib-2.0
SYSCALL_TARGET		= max-syscall
SYSCALL_LIBS		= $(shell pkg-config --libs $(SYSCALL_PACKAGES)) -L../libmax -lmax

AUTOSTART_TEMPLATE	= max-autostart.tmpl
AUTOSTART_TARGET	= max-autostart

CLIENT_OBJECTS		= client.o
CLIENT_TARGET		= max-client
CLIENT_LIBS		= 

KDUMP_TEMPLATE		= max-kdump.tmpl
KDUMP_TARGET		= max-kdump

CFLAGS		= -pipe -ggdb3 -Wall -I. -I.. -I../libmax -DINSTALL="\"$(INSTALL)\"" $(shell pkg-config --cflags $(SYSCALL_PACKAGES))

#all: prereq syscall_all log_all autostart_all client_all kdump_all
all: prereq autostart_all client_all
	#$(MAKE) -f Makefile.modinfo all

#install: syscall_install log_install autostart_install client_install kdump_install
install: autostart_install client_install

#clean: syscall_clean log_clean autostart_clean client_clean kdump_clean
clean: autostart_clean client_clean
	#$(MAKE) -f Makefile.modinfo clean

prereq:
	[ -n "$(INSTALL)" ] || ( echo "INSTALL variable must be set" 1>&2 && false )


syscall_all: $(SYSCALL_OBJECTS)
	$(CC) -o $(SYSCALL_TARGET) $(SYSCALL_OBJECTS) $(SYSCALL_LIBS)
	rm -f $(SYSCALL_OBJECTS)

syscall_install:
	cp -f $(SYSCALL_TARGET) $(INSTALL)
	ln -fs $(INSTALL)/$(SYSCALL_TARGET) /usr/bin

syscall_clean:
	rm -f $(SYSCALL_TARGET)


log_all:
	# Stub for now

log_install:
	# Stub for now

log_clean:
	# Stub for now


autostart_all:
	cat $(AUTOSTART_TEMPLATE) | sed "s|\(^INSTALL=\)\(.*\)$$|\1$(INSTALL)|" >$(AUTOSTART_TARGET)
	chmod +x $(AUTOSTART_TARGET)

autostart_install:
	cp -f $(AUTOSTART_TARGET) $(INSTALL)
	ln -fs $(INSTALL)/$(AUTOSTART_TARGET) /usr/bin

autostart_clean:
	rm -f $(AUTOSTART_TARGET)


client_all: $(CLIENT_OBJECTS)
	$(CC) -o $(CLIENT_TARGET) $(CLIENT_OBJECTS) $(CLIENT_LIBS)
	rm -f $(CLIENT_OBJECTS)

client_install:
	cp -f $(CLIENT_TARGET) $(INSTALL)
	ln -fs $(INSTALL)/$(CLIENT_TARGET) /usr/bin

client_clean:
	rm -f $(CLIENT_TARGET)


kdump_all:
	cat $(KDUMP_TEMPLATE) >$(KDUMP_TARGET)
	chmod +x $(KDUMP_TARGET)

kdump_install:
	cp -f $(KDUMP_TARGET) $(INSTALL)
	ln -fs $(INSTALL)/$(KDUMP_TARGET) /usr/bin

kdump_clean:
	rm -f $(KDUMP_TARGET)


%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o

