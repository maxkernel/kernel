MODEL		= Max 5J

#REMOVE PRIOR TO RELEASE
ifeq ("$(shell whoami)", "dklofas")
INSTALL		= /home/dklofas/Projects/maxkernel/src
else
INSTALL		= /usr/lib/maxkernel
endif
LOGDIR		= /var/log/maxkernel
DBNAME		= kern.db
CONFIG		= max.conf
PROFILE		= yes
RELEASE		= ALPHA

OBJECTS		= kernel.o meta.o module.o profile.o syscall.o io.o syscallblock.o property.o config.o calibration.o buffer.o serialize.o trigger.o exec.o luaenv.o math.o
HEADERS		= kernel.h buffer.h array.h serialize.h 
TARGET		= maxkernel
MODULES		= netui discovery console service httpserver network map serial jpegcompress x264compress nimu maxpod pololu-mssc videre webcam miscserver test
UTILS		= max-kdump max-modinfo max-syscall max-log max-autostart max-client
PACKAGES	= libconfuse libffi glib-2.0 gnet-2.0 gmodule-2.0 sqlite lua5.1 libmatheval
DEFINES		= -D_GNU_SOURCE -DKERNEL $(shell [ "$(PROFILE)" = 'yes' ] && echo "-DEN_PROFILE" ) -D$(RELEASE) -DRELEASE="\"$(RELEASE)\"" -DINSTALL="\"$(INSTALL)\"" -DLOGDIR="\"$(LOGDIR)\"" -DDBNAME="\"$(DBNAME)\"" -DCONFIG="\"$(CONFIG)\"" -DMODEL="\"$(MODEL)\""

LIBS		= $(shell pkg-config --libs $(PACKAGES)) -lbfd -ldl -lrt -laul -Laul
CFLAGS		= -pipe -ggdb3 -Wall $(DEFINES) -I. -Iaul/include $(shell pkg-config --cflags $(PACKAGES))

export INSTALL
export RELEASE

all: prepare prereq $(OBJECTS)
	( $(CC) -o $(TARGET) $(OBJECTS) -rdynamic $(LIBS) 2>>buildlog ) || ( cat buildlog && false )
	$(foreach module,$(MODULES),perl makefile.gen.pl -module $(module) -defines '$(DEFINES)' >$(module)/Makefile &&) true
	( $(foreach module,$(MODULES), echo "In module $(module)" >>buildlog && $(MAKE) -C $(module) 2>>buildlog &&) true ) || ( cat buildlog && false )
	( echo "In libmax" >>buildlog && $(MAKE) -C libmax 2>>buildlog ) || ( cat buildlog && false )
	( echo "In utils" >>buildlog && $(MAKE) -C utils 2>>buildlog) || ( cat buildlog && false )
	cat database.gen.sql | sqlite $(DBNAME) 2>>buildlog
	rm -f $(OBJECTS)
	cat buildlog

clean:
	rm -f $(TARGET) $(OBJECTS) $(DBNAME) buildlog
	( $(foreach module,$(MODULES),$(MAKE) -C $(module) clean; ( cd $(module) && bash clean.part.bash );) ) || true
	$(MAKE) -C aul clean
	$(MAKE) -C libmax clean
	$(MAKE) -C utils clean
	rm -f $(foreach module,$(MODULES),$(module)/Makefile)

install:
	mkdir -p $(INSTALL) $(LOGDIR) $(INSTALL)/modules $(INSTALL)/deploy /usr/include/max
	cp -f $(TARGET) stitcher.lua $(INSTALL)
	( [ -e $(INSTALL)/$(DBNAME) ] || cp -f $(DBNAME) $(INSTALL) )
	cp -rf debug $(INSTALL)
	cp -f $(HEADERS) /usr/include/max
	perl config.gen.pl -install '$(INSTALL)' >$(INSTALL)/$(CONFIG)
	$(foreach module,$(MODULES),$(MAKE) -C $(module) install && ( [ -e $(module)/install.part.bash ] && ( cd $(module) && bash install.part.bash ) || true ) &&) true
	$(MAKE) -C aul install
	$(MAKE) -C libmax install
	cp -f $(foreach util,$(UTILS),utils/$(util)) $(INSTALL)
	$(foreach util,$(UTILS),ln -fs $(INSTALL)/$(util) /usr/bin &&) true
	cat maxkernel.initd | sed "s|\(^INSTALL=\)\(.*\)$$|\1$(INSTALL)|" >/etc/init.d/maxkernel && chmod +x /etc/init.d/maxkernel
	update-rc.d -f maxkernel start 95 2 3 4 5 . stop 95 0 1 6 .
	$(INSTALL)/max-autostart -c
	/etc/init.d/maxkernel restart

rebuild: clean all

prepare:
	echo "Build Log ==----------------------== [$(shell date)]" >buildlog

prereq:
	( echo "In aul" >>buildlog && $(MAKE) -C aul 2>>buildlog ) || ( cat buildlog && false )
	echo "In kernel" >>buildlog

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o 2>>buildlog || ( cat buildlog && false )

