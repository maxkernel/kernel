
#REMOVE PRIOR TO RELEASE
ifeq ("$(shell whoami)", "dklofas")
INSTALL		= /home/dklofas/Projects/maxkernel/src
else
INSTALL		= /usr/lib/maxkernel
endif
LOGDIR		= /var/log/maxkernel
DBNAME		= kern-1.db
CONFIG		= max.conf
MEMFS		= memfs
RELEASE		= BETA

MODEL       = Max 5J

MODULES		= console discovery netui httpserver map service drivemodel lookmodel webcam ssc-32 parallax-ssc gps network maxpod jpegcompress
UTILS		= autostart client syscall
#OLD_UTILS	= kdump modinfo log
HEADERS		= kernel.h kernel-types.h buffer.h array.h serialize.h method.h

SRCS		= kernel.c meta.c module.c memfs.c function.c syscall.c io.c syscallblock.c property.c config.c parse.c calibration.c buffer.c serialize.c trigger.c exec.c luaenv.c
OBJS		= $(SRCS:.c=.o)
PACKAGES	= libconfuse libffi glib-2.0 sqlite3 lua5.1
INCLUDES	= -I. -Iaul/include $(shell pkg-config --cflags-only-I $(PACKAGES))
DEFINES		= -D_GNU_SOURCE -DKERNEL -D$(RELEASE) -DRELEASE="\"$(RELEASE)\"" -DINSTALL="\"$(INSTALL)\"" -DLOGDIR="\"$(LOGDIR)\"" -DDBNAME="\"$(DBNAME)\"" -DCONFIG="\"$(CONFIG)\"" -DMEMFS="\"$(MEMFS)\""
CFLAGS		= -pipe -ggdb3 -Wall -std=gnu99 $(shell pkg-config --cflags-only-other $(PACKAGES))
LIBS		= $(shell pkg-config --libs $(PACKAGES)) -laul  -lbfd -ldl -lrt
LFLAGS		= -Laul -Wl,--export-dynamic

TARGET		= maxkernel


export INSTALL
export RELEASE

.PHONY: prepare prereq body all install clean rebuild depend

all: prereq body $(TARGET)
	$(foreach module,$(MODULES),python makefile.gen.py --module $(module) --defines '$(DEFINES)' >$(module)/Makefile && $(MAKE) -C $(module) depend &&) true
	( $(foreach module,$(MODULES), echo "In module $(module)" >>buildlog && $(MAKE) -C $(module) all 2>>buildlog &&) true ) || ( cat buildlog && false )
	( echo "In libmax" >>buildlog && $(MAKE) -C libmax all 2>>buildlog ) || ( cat buildlog && false )
	( echo "In testmax" >> buildlog && $(MAKE) -C testmax all 2>>buildlog ) || ( cat buildlog && false )
	( echo "In utils" >>buildlog && $(foreach util,$(UTILS), $(MAKE) -C utils -f Makefile.$(util) all 2>>buildlog &&) true ) || ( cat buildlog && false )
	( echo "In gendb" >>buildlog && cat database.gen.sql | sqlite3 $(DBNAME) >>buildlog ) || ( cat buildlog && false )
	( echo "Done (success)" >>buildlog && cat buildlog )

body:
	echo "In kernel" >>buildlog

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

install:
	mkdir -p $(INSTALL) $(LOGDIR) $(INSTALL)/modules $(INSTALL)/stitcher $(INSTALL)/$(MEMFS) /usr/include/maxkernel
	cp -f $(TARGET) $(INSTALL)
	( [ -e $(INSTALL)/$(DBNAME) ] || cp -f $(DBNAME) $(INSTALL) )
	cp -f stitcher/*.lua $(INSTALL)/stitcher
	cp -f $(HEADERS) /usr/include/maxkernel
	python maxconf.gen.py --install '$(INSTALL)' --model '$(MODEL)' >$(INSTALL)/$(CONFIG)
	$(MAKE) -C aul install
	$(MAKE) -C libmax install
	$(MAKE) -C testmax install
	$(foreach util,$(UTILS), $(MAKE) -C utils -f Makefile.$(util) install &&) true
	$(foreach module,$(MODULES),$(MAKE) -C $(module) install && ( [ -e $(module)/install.part.bash ] && ( cd $(module) && bash install.part.bash ) || true ) &&) true
	cat maxkernel.initd | sed "s|\(^INSTALL=\)\(.*\)$$|\1$(INSTALL)|" >/etc/init.d/maxkernel && chmod +x /etc/init.d/maxkernel
	update-rc.d -f maxkernel start 95 2 3 4 5 . stop 95 0 1 6 .
	$(INSTALL)/max-autostart -c
	/etc/init.d/maxkernel restart

clean:
	( $(foreach module,$(MODULES),$(MAKE) -C $(module) clean; ( cd $(module) && bash clean.part.bash );) ) || true
	$(MAKE) -C aul clean
	$(MAKE) -C libmax clean
	$(MAKE) -C testmax clean
	$(foreach util,$(UTILS), $(MAKE) -C utils -f Makefile.$(util) clean &&) true
	rm -f $(TARGET) $(OBJS) $(foreach module,$(MODULES),$(module)/Makefile)

rebuild: clean all

depend: $(SRCS)
	makedepend $(DEFINES) $(INCLUDES) $^ 2>/dev/null
	$(MAKE) -C aul depend
	$(MAKE) -C testmax depend

prepare:
	echo "Build Log ==----------------------== [$(shell date)]" >buildlog

prereq: prepare
	( echo "In aul" >>buildlog && $(MAKE) -C aul all 2>>buildlog ) || ( cat buildlog && false )

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@ 2>>buildlog || ( cat buildlog && false )

# DO NOT DELETE THIS LINE -- make depend needs it

