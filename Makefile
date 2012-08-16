INSTALL		= /usr/lib/maxkernel
LOGDIR		= /var/log/maxkernel
DBNAME		= kern-1.1.db
CONFIG		= max.conf
MEMFS		= memfs

VERSION		= 1.2
RELEASE		= BETA
MODEL		= Robot

UTILS		= autostart client syscall
#OLD_UTILS	= kdump modinfo log
HEADERS		= kernel.h kernel-types.h buffer.h array.h serialize.h method.h

SRCS		= kernel.c module.c memfs.c path.c function.c syscall.c block.c blockinst.c rategroup.c port.c link.c iobacking.c syscallblock.c property.c config.c calibration.c buffer.c serialize.c trigger.c
#TODO - add -Wextra to CFLAGS
PACKAGES	= libconfuse libffi sqlite3
INCLUDES	= -I. -Iaul/include -Ilibmodel/include $(shell pkg-config --cflags-only-I $(PACKAGES))
DEFINES		= -D_GNU_SOURCE -DKERNEL -DUSE_BFD -DUSE_DL -DUSE_LUA -D$(RELEASE) -DVERSION="\"$(VERSION)\"" -DRELEASE="\"$(RELEASE)\"" -DINSTALL="\"$(INSTALL)\"" -DLOGDIR="\"$(LOGDIR)\"" -DDBNAME="\"$(DBNAME)\"" -DCONFIG="\"$(CONFIG)\"" -DMEMFS="\"$(MEMFS)\""
CFLAGS		= -pipe -ggdb3 -Wall -Wextra -O2 -std=gnu99 $(shell pkg-config --cflags-only-other $(PACKAGES))
LIBS		= $(shell pkg-config --libs $(PACKAGES)) -laul -lmaxmodel -lbfd -ldl -lrt
LFLAGS		= -Laul -Llibmodel -Wl,--export-dynamic

TARGET		= maxkernel


export INSTALL
export VERSION
export RELEASE

.PHONY: prepare prereq body all install clean rebuild depend docs

OBJS		= $(SRCS:.c=.o)


all: prereq body $(TARGET)
	( echo "In modules" >>buildlog && $(MAKE) -C modules all 2>>buildlog ) || ( cat buildlog && false )
	( echo "In libmax" >>buildlog && $(MAKE) -C libmax all 2>>buildlog ) || ( cat buildlog && false )
	( echo "In testmax" >> buildlog && $(MAKE) -C testmax all 2>>buildlog ) || ( cat buildlog && false )
	( echo "In utils" >>buildlog && $(foreach util,$(UTILS), $(MAKE) -C utils -f Makefile.$(util) all 2>>buildlog &&) true ) || ( cat buildlog && false )
	( echo "In gendb" >>buildlog && cat database.gen.sql | sqlite3 $(DBNAME) >>buildlog ) || ( cat buildlog && false )
	( echo "Done (success)" >>buildlog && cat buildlog )

body:
	echo "In kernel" >>buildlog

$(TARGET): $(OBJS)
	( $(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS) 2>>buildlog ) || ( cat buildlog && false )

install:
	mkdir -p $(INSTALL) $(LOGDIR) $(INSTALL)/modules $(INSTALL)/stitcher $(INSTALL)/$(MEMFS) /usr/include/maxkernel
	cp -f $(TARGET) $(INSTALL)
	( [ -e $(INSTALL)/$(DBNAME) ] || cp -f $(DBNAME) $(INSTALL) )
	cp -f $(HEADERS) /usr/include/maxkernel
	python maxconf.gen.py --install '$(INSTALL)' --model '$(MODEL)' >$(INSTALL)/$(CONFIG)
	$(MAKE) -C aul install
	$(MAKE) -C modules install
	$(MAKE) -C libmodel install
	$(MAKE) -C libmax install
	$(MAKE) -C testmax install
	$(foreach util,$(UTILS), $(MAKE) -C utils -f Makefile.$(util) install &&) true
	
	cat maxkernel.initd | sed "s|\(^INSTALL=\)\(.*\)$$|\1$(INSTALL)|" >/etc/init.d/maxkernel && chmod +x /etc/init.d/maxkernel
	update-rc.d -f maxkernel start 95 2 3 4 5 . stop 95 0 1 6 .
	$(INSTALL)/max-autostart -c
	/etc/init.d/maxkernel restart

clean:
	$(MAKE) -C aul clean
	$(MAKE) -C modules clean
	$(MAKE) -C libmodel clean
	$(MAKE) -C libmax clean
	$(MAKE) -C testmax clean
	$(foreach util,$(UTILS), $(MAKE) -C utils -f Makefile.$(util) clean &&) true
	rm -rf $(TARGET) $(OBJS) doxygen

rebuild: clean all

depend: $(SRCS)
	makedepend $(DEFINES) $(INCLUDES) $^ 2>/dev/null
	$(MAKE) -C aul depend
	$(MAKE) -C libmodel depend
	$(MAKE) -C testmax depend

docs:
	mkdir -p doxygen
	python docsroot.gen.py >doxygen/index.html
	$(MAKE) -C libmax docs
	cp -r libmax/docs doxygen/libmax

prepare:
	echo "Kernel build log ==----------------------== [$(shell date)]" >buildlog

prereq: prepare
	( echo "In aul" >>buildlog && $(MAKE) -C aul all 2>>buildlog ) || ( cat buildlog && false )
	( echo "In model" >>buildlog && $(MAKE) -C libmodel all 2>>buildlog ) || ( cat buildlog && false )

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@ 2>>buildlog || ( cat buildlog && false )

# DO NOT DELETE THIS LINE -- make depend needs it

