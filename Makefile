MODEL		= Max 5J

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
PROFILE		= yes
RELEASE		= ALPHA

MODULES		= console netui httpserver ssc-32 map gps discovery
OLD_MODULES = service network jpegcompress propeller-ssc webcam maxpod test
UTILS		= 
OLD_UTILS	= syscall client autostart kdump modinfo log
HEADERS		= kernel.h buffer.h array.h serialize.h 

SRCS		= kernel.c meta.c module.c profile.c memfs.c syscall.c io.c syscallblock.c property.c config.c calibration.c buffer2.c serialize.c trigger.c exec.c luaenv.c math.c
OBJS		= $(SRCS:.c=.o)
PACKAGES	= libconfuse libffi glib-2.0 sqlite3 lua5.1 libmatheval
INCLUDES	= -I. -Iaul/include $(shell pkg-config --cflags-only-I $(PACKAGES))
DEFINES		= -D_GNU_SOURCE -DKERNEL $(shell [ "$(PROFILE)" = 'yes' ] && echo "-DEN_PROFILE" ) -D$(RELEASE) -DRELEASE="\"$(RELEASE)\"" -DINSTALL="\"$(INSTALL)\"" -DLOGDIR="\"$(LOGDIR)\"" -DDBNAME="\"$(DBNAME)\"" -DCONFIG="\"$(CONFIG)\"" -DMEMFS="\"$(MEMFS)\"" -DMODEL="\"$(MODEL)\""
CFLAGS		= -pipe -ggdb3 -Wall $(shell pkg-config --cflags-only-other $(PACKAGES))
LIBS		= $(shell pkg-config --libs $(PACKAGES)) -laul  -lbfd -ldl -lrt
LFLAGS		= -Laul -Wl,--export-dynamic

TARGET		= maxkernel

# IN DEVEL MOD = x264compress nimu pololu-mssc videre miscserver
#UTILS		= max-kdump max-modinfo max-syscall max-log max-autostart max-client

# TODO ========
# Replace buffer_t with memfs				(Working)
# Re-work calibration infrastructure
# Replace select with epoll in AUL mainloop		(DONE)
# Write new maxlib with integrated services		(Working)
#   - Write kernel-side service streamer
# Modify AUL queue to use circular queue		(DONE)
# Add profiling support
# Add menuconfig option to makefile
# Fix up serialize
# Modify the map module to use proper regex
# Modify the meta file to use proper regex
# Fix utilities
# Update netui to add set buttons in calibration pane


export INSTALL
export RELEASE

.PHONY: prepare prereq install clean rebuild depend unittest

all: prepare prereq $(TARGET)
	$(foreach module,$(MODULES),perl makefile.gen.pl -module $(module) -defines '$(DEFINES)' >$(module)/Makefile && $(MAKE) -C $(module) depend &&) true
	( $(foreach module,$(MODULES), echo "In module $(module)" >>buildlog && $(MAKE) -C $(module) 2>>buildlog &&) true ) || ( cat buildlog && false )
	( echo "In libmax" >>buildlog && $(MAKE) -C libmax 2>>buildlog ) || ( cat buildlog && false )
	( echo "In utils" >>buildlog && $(foreach util,$(UTILS), $(MAKE) -C utils Makefile.$(util) 2>>buildlog &&) true ) || ( cat buildlog && false )
	( echo "In gendb" >>buildlog && cat database.gen.sql | sqlite3 $(DBNAME) >>buildlog ) || ( cat buildlog && false )
	cat buildlog

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

install:
	mkdir -p $(INSTALL) $(LOGDIR) $(INSTALL)/modules $(INSTALL)/$(MEMFS) /usr/include/max
	cp -f $(TARGET) $(INSTALL)
	( [ -e $(INSTALL)/$(DBNAME) ] || cp -f $(DBNAME) $(INSTALL) )
	cp -rf debug stitcher $(INSTALL)
	cp -f $(HEADERS) /usr/include/max
	perl config.gen.pl -install '$(INSTALL)' >$(INSTALL)/$(CONFIG)
	$(foreach module,$(MODULES),$(MAKE) -C $(module) install && ( [ -e $(module)/install.part.bash ] && ( cd $(module) && bash install.part.bash ) || true ) &&) true
	$(MAKE) -C aul install
	$(MAKE) -C libmax install
	$(MAKE) -C utils install
	cat maxkernel.initd | sed "s|\(^INSTALL=\)\(.*\)$$|\1$(INSTALL)|" >/etc/init.d/maxkernel && chmod +x /etc/init.d/maxkernel
	update-rc.d -f maxkernel start 95 2 3 4 5 . stop 95 0 1 6 .
	$(INSTALL)/max-autostart -c
	/etc/init.d/maxkernel restart

clean:
	( $(foreach module,$(MODULES),$(MAKE) -C $(module) clean; ( cd $(module) && bash clean.part.bash );) ) || true
	$(MAKE) -C aul clean
	$(MAKE) -C libmax clean
	$(MAKE) -C utils clean
	$(MAKE) -C unittest clean
	rm -f $(TARGET) $(OBJS) $(foreach module,$(MODULES),$(module)/Makefile)

rebuild: clean all

depend: $(SRCS)
	makedepend $(DEFINES) $(INCLUDES) $^ 2>/dev/null
	$(MAKE) -C aul depend
	$(MAKE) -C libmax depend
	$(MAKE) -C unittest depend

unittest: prereq
	$(MAKE) -C unittest run

prepare:
	echo "Build Log ==----------------------== [$(shell date)]" >buildlog

prereq:
	( echo "In aul" >>buildlog && $(MAKE) -C aul 2>>buildlog ) || ( cat buildlog && false )

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@ 2>>buildlog || ( cat buildlog && false )

#all: prepare prereq $(OBJECTS)
#	( $(CC) -o $(TARGET) $(OBJECTS) -rdynamic $(LIBS) 2>>buildlog ) || ( cat buildlog && false )
#	$(foreach module,$(MODULES),perl makefile.gen.pl -module $(module) -defines '$(DEFINES)' >$(module)/Makefile &&) true
#	( $(foreach module,$(MODULES), echo "In module $(module)" >>buildlog && $(MAKE) -C $(module) 2>>buildlog &&) true ) || ( cat buildlog && false )
#	( echo "In libmax" >>buildlog && $(MAKE) -C libmax 2>>buildlog ) || ( cat buildlog && false )
#	( echo "In utils" >>buildlog && $(MAKE) -C utils 2>>buildlog) || ( cat buildlog && false )
#	cat database.gen.sql | sqlite $(DBNAME) 2>>buildlog
#	rm -f $(OBJECTS)
#	cat buildlog

#clean:
#	rm -f $(TARGET) $(OBJECTS) $(DBNAME) buildlog
#	( $(foreach module,$(MODULES),$(MAKE) -C $(module) clean; ( cd $(module) && bash clean.part.bash );) ) || true
#	$(MAKE) -C aul clean
#	$(MAKE) -C libmax clean
#	$(MAKE) -C utils clean
#	rm -f $(foreach module,$(MODULES),$(module)/Makefile)

#install:
#	mkdir -p $(INSTALL) $(LOGDIR) $(INSTALL)/modules $(INSTALL)/deploy /usr/include/max
#	cp -f $(TARGET) stitcher.lua $(INSTALL)
#	( [ -e $(INSTALL)/$(DBNAME) ] || cp -f $(DBNAME) $(INSTALL) )
#	cp -rf debug $(INSTALL)
#	cp -f $(HEADERS) /usr/include/max
#	perl config.gen.pl -install '$(INSTALL)' >$(INSTALL)/$(CONFIG)
#	$(foreach module,$(MODULES),$(MAKE) -C $(module) install && ( [ -e $(module)/install.part.bash ] && ( cd $(module) && bash install.part.bash ) || true ) &&) true
#	$(MAKE) -C aul install
#	$(MAKE) -C libmax install
#	cp -f $(foreach util,$(UTILS),utils/$(util)) $(INSTALL)
#	$(foreach util,$(UTILS),ln -fs $(INSTALL)/$(util) /usr/bin &&) true
#	cat maxkernel.initd | sed "s|\(^INSTALL=\)\(.*\)$$|\1$(INSTALL)|" >/etc/init.d/maxkernel && chmod +x /etc/init.d/maxkernel
#	update-rc.d -f maxkernel start 95 2 3 4 5 . stop 95 0 1 6 .
#	$(INSTALL)/max-autostart -c
#	/etc/init.d/maxkernel restart

#rebuild: clean all

#prepare:
#	echo "Build Log ==----------------------== [$(shell date)]" >buildlog

#prereq:
#	( echo "In aul" >>buildlog && $(MAKE) -C aul 2>>buildlog ) || ( cat buildlog && false )
#	echo "In kernel" >>buildlog

#%.o: %.c
#	$(CC) -c $(CFLAGS) $*.c -o $*.o 2>>buildlog || ( cat buildlog && false )

# DO NOT DELETE THIS LINE -- make depend needs it

kernel.o: /usr/include/glib-2.0/glib.h /usr/include/glib-2.0/glib/galloca.h
kernel.o: /usr/include/glib-2.0/glib/gtypes.h
kernel.o: /usr/lib/glib-2.0/include/glibconfig.h
kernel.o: /usr/include/glib-2.0/glib/gmacros.h
kernel.o: /usr/include/glib-2.0/glib/garray.h
kernel.o: /usr/include/glib-2.0/glib/gasyncqueue.h
kernel.o: /usr/include/glib-2.0/glib/gthread.h
kernel.o: /usr/include/glib-2.0/glib/gerror.h
kernel.o: /usr/include/glib-2.0/glib/gquark.h
kernel.o: /usr/include/glib-2.0/glib/gutils.h
kernel.o: /usr/include/glib-2.0/glib/gatomic.h
kernel.o: /usr/include/glib-2.0/glib/gbacktrace.h
kernel.o: /usr/include/glib-2.0/glib/gbase64.h
kernel.o: /usr/include/glib-2.0/glib/gbitlock.h
kernel.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
kernel.o: /usr/include/glib-2.0/glib/gcache.h
kernel.o: /usr/include/glib-2.0/glib/glist.h
kernel.o: /usr/include/glib-2.0/glib/gmem.h
kernel.o: /usr/include/glib-2.0/glib/gslice.h
kernel.o: /usr/include/glib-2.0/glib/gchecksum.h
kernel.o: /usr/include/glib-2.0/glib/gcompletion.h
kernel.o: /usr/include/glib-2.0/glib/gconvert.h
kernel.o: /usr/include/glib-2.0/glib/gdataset.h
kernel.o: /usr/include/glib-2.0/glib/gdate.h
kernel.o: /usr/include/glib-2.0/glib/gdir.h
kernel.o: /usr/include/glib-2.0/glib/gfileutils.h
kernel.o: /usr/include/glib-2.0/glib/ghash.h
kernel.o: /usr/include/glib-2.0/glib/ghook.h
kernel.o: /usr/include/glib-2.0/glib/ghostutils.h
kernel.o: /usr/include/glib-2.0/glib/giochannel.h
kernel.o: /usr/include/glib-2.0/glib/gmain.h
kernel.o: /usr/include/glib-2.0/glib/gpoll.h
kernel.o: /usr/include/glib-2.0/glib/gslist.h
kernel.o: /usr/include/glib-2.0/glib/gstring.h
kernel.o: /usr/include/glib-2.0/glib/gunicode.h
kernel.o: /usr/include/glib-2.0/glib/gkeyfile.h
kernel.o: /usr/include/glib-2.0/glib/gmappedfile.h
kernel.o: /usr/include/glib-2.0/glib/gmarkup.h
kernel.o: /usr/include/glib-2.0/glib/gmessages.h
kernel.o: /usr/include/glib-2.0/glib/gnode.h
kernel.o: /usr/include/glib-2.0/glib/goption.h
kernel.o: /usr/include/glib-2.0/glib/gpattern.h
kernel.o: /usr/include/glib-2.0/glib/gprimes.h
kernel.o: /usr/include/glib-2.0/glib/gqsort.h
kernel.o: /usr/include/glib-2.0/glib/gqueue.h
kernel.o: /usr/include/glib-2.0/glib/grand.h
kernel.o: /usr/include/glib-2.0/glib/grel.h
kernel.o: /usr/include/glib-2.0/glib/gregex.h
kernel.o: /usr/include/glib-2.0/glib/gscanner.h
kernel.o: /usr/include/glib-2.0/glib/gsequence.h
kernel.o: /usr/include/glib-2.0/glib/gshell.h
kernel.o: /usr/include/glib-2.0/glib/gspawn.h
kernel.o: /usr/include/glib-2.0/glib/gstrfuncs.h
kernel.o: /usr/include/glib-2.0/glib/gtestutils.h
kernel.o: /usr/include/glib-2.0/glib/gthreadpool.h
kernel.o: /usr/include/glib-2.0/glib/gtimer.h
kernel.o: /usr/include/glib-2.0/glib/gtree.h
kernel.o: /usr/include/glib-2.0/glib/gurifuncs.h
kernel.o: /usr/include/glib-2.0/glib/gvarianttype.h
kernel.o: /usr/include/glib-2.0/glib/gvariant.h aul/include/aul/common.h
kernel.o: aul/include/aul/list.h aul/include/aul/hashtable.h
kernel.o: aul/include/aul/mainloop.h aul/include/aul/mutex.h ./kernel.h
kernel.o: aul/include/aul/log.h aul/include/aul/exception.h
kernel.o: aul/include/aul/string.h ./kernel-priv.h
meta.o: /usr/include/glib-2.0/glib.h /usr/include/glib-2.0/glib/galloca.h
meta.o: /usr/include/glib-2.0/glib/gtypes.h
meta.o: /usr/lib/glib-2.0/include/glibconfig.h
meta.o: /usr/include/glib-2.0/glib/gmacros.h
meta.o: /usr/include/glib-2.0/glib/garray.h
meta.o: /usr/include/glib-2.0/glib/gasyncqueue.h
meta.o: /usr/include/glib-2.0/glib/gthread.h
meta.o: /usr/include/glib-2.0/glib/gerror.h
meta.o: /usr/include/glib-2.0/glib/gquark.h
meta.o: /usr/include/glib-2.0/glib/gutils.h
meta.o: /usr/include/glib-2.0/glib/gatomic.h
meta.o: /usr/include/glib-2.0/glib/gbacktrace.h
meta.o: /usr/include/glib-2.0/glib/gbase64.h
meta.o: /usr/include/glib-2.0/glib/gbitlock.h
meta.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
meta.o: /usr/include/glib-2.0/glib/gcache.h
meta.o: /usr/include/glib-2.0/glib/glist.h /usr/include/glib-2.0/glib/gmem.h
meta.o: /usr/include/glib-2.0/glib/gslice.h
meta.o: /usr/include/glib-2.0/glib/gchecksum.h
meta.o: /usr/include/glib-2.0/glib/gcompletion.h
meta.o: /usr/include/glib-2.0/glib/gconvert.h
meta.o: /usr/include/glib-2.0/glib/gdataset.h
meta.o: /usr/include/glib-2.0/glib/gdate.h /usr/include/glib-2.0/glib/gdir.h
meta.o: /usr/include/glib-2.0/glib/gfileutils.h
meta.o: /usr/include/glib-2.0/glib/ghash.h /usr/include/glib-2.0/glib/ghook.h
meta.o: /usr/include/glib-2.0/glib/ghostutils.h
meta.o: /usr/include/glib-2.0/glib/giochannel.h
meta.o: /usr/include/glib-2.0/glib/gmain.h /usr/include/glib-2.0/glib/gpoll.h
meta.o: /usr/include/glib-2.0/glib/gslist.h
meta.o: /usr/include/glib-2.0/glib/gstring.h
meta.o: /usr/include/glib-2.0/glib/gunicode.h
meta.o: /usr/include/glib-2.0/glib/gkeyfile.h
meta.o: /usr/include/glib-2.0/glib/gmappedfile.h
meta.o: /usr/include/glib-2.0/glib/gmarkup.h
meta.o: /usr/include/glib-2.0/glib/gmessages.h
meta.o: /usr/include/glib-2.0/glib/gnode.h
meta.o: /usr/include/glib-2.0/glib/goption.h
meta.o: /usr/include/glib-2.0/glib/gpattern.h
meta.o: /usr/include/glib-2.0/glib/gprimes.h
meta.o: /usr/include/glib-2.0/glib/gqsort.h
meta.o: /usr/include/glib-2.0/glib/gqueue.h
meta.o: /usr/include/glib-2.0/glib/grand.h /usr/include/glib-2.0/glib/grel.h
meta.o: /usr/include/glib-2.0/glib/gregex.h
meta.o: /usr/include/glib-2.0/glib/gscanner.h
meta.o: /usr/include/glib-2.0/glib/gsequence.h
meta.o: /usr/include/glib-2.0/glib/gshell.h
meta.o: /usr/include/glib-2.0/glib/gspawn.h
meta.o: /usr/include/glib-2.0/glib/gstrfuncs.h
meta.o: /usr/include/glib-2.0/glib/gtestutils.h
meta.o: /usr/include/glib-2.0/glib/gthreadpool.h
meta.o: /usr/include/glib-2.0/glib/gtimer.h
meta.o: /usr/include/glib-2.0/glib/gtree.h
meta.o: /usr/include/glib-2.0/glib/gurifuncs.h
meta.o: /usr/include/glib-2.0/glib/gvarianttype.h
meta.o: /usr/include/glib-2.0/glib/gvariant.h ./kernel.h
meta.o: aul/include/aul/common.h aul/include/aul/log.h
meta.o: aul/include/aul/exception.h aul/include/aul/string.h
meta.o: aul/include/aul/list.h ./kernel-priv.h aul/include/aul/mainloop.h
meta.o: aul/include/aul/mutex.h aul/include/aul/hashtable.h
module.o: /usr/include/glib-2.0/glib.h /usr/include/glib-2.0/glib/galloca.h
module.o: /usr/include/glib-2.0/glib/gtypes.h
module.o: /usr/lib/glib-2.0/include/glibconfig.h
module.o: /usr/include/glib-2.0/glib/gmacros.h
module.o: /usr/include/glib-2.0/glib/garray.h
module.o: /usr/include/glib-2.0/glib/gasyncqueue.h
module.o: /usr/include/glib-2.0/glib/gthread.h
module.o: /usr/include/glib-2.0/glib/gerror.h
module.o: /usr/include/glib-2.0/glib/gquark.h
module.o: /usr/include/glib-2.0/glib/gutils.h
module.o: /usr/include/glib-2.0/glib/gatomic.h
module.o: /usr/include/glib-2.0/glib/gbacktrace.h
module.o: /usr/include/glib-2.0/glib/gbase64.h
module.o: /usr/include/glib-2.0/glib/gbitlock.h
module.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
module.o: /usr/include/glib-2.0/glib/gcache.h
module.o: /usr/include/glib-2.0/glib/glist.h
module.o: /usr/include/glib-2.0/glib/gmem.h
module.o: /usr/include/glib-2.0/glib/gslice.h
module.o: /usr/include/glib-2.0/glib/gchecksum.h
module.o: /usr/include/glib-2.0/glib/gcompletion.h
module.o: /usr/include/glib-2.0/glib/gconvert.h
module.o: /usr/include/glib-2.0/glib/gdataset.h
module.o: /usr/include/glib-2.0/glib/gdate.h
module.o: /usr/include/glib-2.0/glib/gdir.h
module.o: /usr/include/glib-2.0/glib/gfileutils.h
module.o: /usr/include/glib-2.0/glib/ghash.h
module.o: /usr/include/glib-2.0/glib/ghook.h
module.o: /usr/include/glib-2.0/glib/ghostutils.h
module.o: /usr/include/glib-2.0/glib/giochannel.h
module.o: /usr/include/glib-2.0/glib/gmain.h
module.o: /usr/include/glib-2.0/glib/gpoll.h
module.o: /usr/include/glib-2.0/glib/gslist.h
module.o: /usr/include/glib-2.0/glib/gstring.h
module.o: /usr/include/glib-2.0/glib/gunicode.h
module.o: /usr/include/glib-2.0/glib/gkeyfile.h
module.o: /usr/include/glib-2.0/glib/gmappedfile.h
module.o: /usr/include/glib-2.0/glib/gmarkup.h
module.o: /usr/include/glib-2.0/glib/gmessages.h
module.o: /usr/include/glib-2.0/glib/gnode.h
module.o: /usr/include/glib-2.0/glib/goption.h
module.o: /usr/include/glib-2.0/glib/gpattern.h
module.o: /usr/include/glib-2.0/glib/gprimes.h
module.o: /usr/include/glib-2.0/glib/gqsort.h
module.o: /usr/include/glib-2.0/glib/gqueue.h
module.o: /usr/include/glib-2.0/glib/grand.h
module.o: /usr/include/glib-2.0/glib/grel.h
module.o: /usr/include/glib-2.0/glib/gregex.h
module.o: /usr/include/glib-2.0/glib/gscanner.h
module.o: /usr/include/glib-2.0/glib/gsequence.h
module.o: /usr/include/glib-2.0/glib/gshell.h
module.o: /usr/include/glib-2.0/glib/gspawn.h
module.o: /usr/include/glib-2.0/glib/gstrfuncs.h
module.o: /usr/include/glib-2.0/glib/gtestutils.h
module.o: /usr/include/glib-2.0/glib/gthreadpool.h
module.o: /usr/include/glib-2.0/glib/gtimer.h
module.o: /usr/include/glib-2.0/glib/gtree.h
module.o: /usr/include/glib-2.0/glib/gurifuncs.h
module.o: /usr/include/glib-2.0/glib/gvarianttype.h
module.o: /usr/include/glib-2.0/glib/gvariant.h ./kernel.h
module.o: aul/include/aul/common.h aul/include/aul/log.h
module.o: aul/include/aul/exception.h aul/include/aul/string.h
module.o: aul/include/aul/list.h ./kernel-priv.h aul/include/aul/mainloop.h
module.o: aul/include/aul/mutex.h aul/include/aul/hashtable.h
profile.o: ./kernel.h aul/include/aul/common.h aul/include/aul/log.h
profile.o: aul/include/aul/exception.h aul/include/aul/string.h
profile.o: aul/include/aul/list.h ./kernel-priv.h aul/include/aul/mainloop.h
profile.o: aul/include/aul/mutex.h aul/include/aul/hashtable.h
memfs.o: aul/include/aul/string.h aul/include/aul/common.h
memfs.o: aul/include/aul/exception.h ./kernel.h aul/include/aul/log.h
memfs.o: aul/include/aul/list.h ./kernel-priv.h aul/include/aul/mainloop.h
memfs.o: aul/include/aul/mutex.h aul/include/aul/hashtable.h
syscall.o: /usr/include/glib-2.0/glib.h /usr/include/glib-2.0/glib/galloca.h
syscall.o: /usr/include/glib-2.0/glib/gtypes.h
syscall.o: /usr/lib/glib-2.0/include/glibconfig.h
syscall.o: /usr/include/glib-2.0/glib/gmacros.h
syscall.o: /usr/include/glib-2.0/glib/garray.h
syscall.o: /usr/include/glib-2.0/glib/gasyncqueue.h
syscall.o: /usr/include/glib-2.0/glib/gthread.h
syscall.o: /usr/include/glib-2.0/glib/gerror.h
syscall.o: /usr/include/glib-2.0/glib/gquark.h
syscall.o: /usr/include/glib-2.0/glib/gutils.h
syscall.o: /usr/include/glib-2.0/glib/gatomic.h
syscall.o: /usr/include/glib-2.0/glib/gbacktrace.h
syscall.o: /usr/include/glib-2.0/glib/gbase64.h
syscall.o: /usr/include/glib-2.0/glib/gbitlock.h
syscall.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
syscall.o: /usr/include/glib-2.0/glib/gcache.h
syscall.o: /usr/include/glib-2.0/glib/glist.h
syscall.o: /usr/include/glib-2.0/glib/gmem.h
syscall.o: /usr/include/glib-2.0/glib/gslice.h
syscall.o: /usr/include/glib-2.0/glib/gchecksum.h
syscall.o: /usr/include/glib-2.0/glib/gcompletion.h
syscall.o: /usr/include/glib-2.0/glib/gconvert.h
syscall.o: /usr/include/glib-2.0/glib/gdataset.h
syscall.o: /usr/include/glib-2.0/glib/gdate.h
syscall.o: /usr/include/glib-2.0/glib/gdir.h
syscall.o: /usr/include/glib-2.0/glib/gfileutils.h
syscall.o: /usr/include/glib-2.0/glib/ghash.h
syscall.o: /usr/include/glib-2.0/glib/ghook.h
syscall.o: /usr/include/glib-2.0/glib/ghostutils.h
syscall.o: /usr/include/glib-2.0/glib/giochannel.h
syscall.o: /usr/include/glib-2.0/glib/gmain.h
syscall.o: /usr/include/glib-2.0/glib/gpoll.h
syscall.o: /usr/include/glib-2.0/glib/gslist.h
syscall.o: /usr/include/glib-2.0/glib/gstring.h
syscall.o: /usr/include/glib-2.0/glib/gunicode.h
syscall.o: /usr/include/glib-2.0/glib/gkeyfile.h
syscall.o: /usr/include/glib-2.0/glib/gmappedfile.h
syscall.o: /usr/include/glib-2.0/glib/gmarkup.h
syscall.o: /usr/include/glib-2.0/glib/gmessages.h
syscall.o: /usr/include/glib-2.0/glib/gnode.h
syscall.o: /usr/include/glib-2.0/glib/goption.h
syscall.o: /usr/include/glib-2.0/glib/gpattern.h
syscall.o: /usr/include/glib-2.0/glib/gprimes.h
syscall.o: /usr/include/glib-2.0/glib/gqsort.h
syscall.o: /usr/include/glib-2.0/glib/gqueue.h
syscall.o: /usr/include/glib-2.0/glib/grand.h
syscall.o: /usr/include/glib-2.0/glib/grel.h
syscall.o: /usr/include/glib-2.0/glib/gregex.h
syscall.o: /usr/include/glib-2.0/glib/gscanner.h
syscall.o: /usr/include/glib-2.0/glib/gsequence.h
syscall.o: /usr/include/glib-2.0/glib/gshell.h
syscall.o: /usr/include/glib-2.0/glib/gspawn.h
syscall.o: /usr/include/glib-2.0/glib/gstrfuncs.h
syscall.o: /usr/include/glib-2.0/glib/gtestutils.h
syscall.o: /usr/include/glib-2.0/glib/gthreadpool.h
syscall.o: /usr/include/glib-2.0/glib/gtimer.h
syscall.o: /usr/include/glib-2.0/glib/gtree.h
syscall.o: /usr/include/glib-2.0/glib/gurifuncs.h
syscall.o: /usr/include/glib-2.0/glib/gvarianttype.h
syscall.o: /usr/include/glib-2.0/glib/gvariant.h ./kernel.h
syscall.o: aul/include/aul/common.h aul/include/aul/log.h
syscall.o: aul/include/aul/exception.h aul/include/aul/string.h
syscall.o: aul/include/aul/list.h ./kernel-priv.h aul/include/aul/mainloop.h
syscall.o: aul/include/aul/mutex.h aul/include/aul/hashtable.h array.h
syscall.o: ./buffer.h ./buffer2.h serialize.h
io.o: /usr/include/glib-2.0/glib.h /usr/include/glib-2.0/glib/galloca.h
io.o: /usr/include/glib-2.0/glib/gtypes.h
io.o: /usr/lib/glib-2.0/include/glibconfig.h
io.o: /usr/include/glib-2.0/glib/gmacros.h
io.o: /usr/include/glib-2.0/glib/garray.h
io.o: /usr/include/glib-2.0/glib/gasyncqueue.h
io.o: /usr/include/glib-2.0/glib/gthread.h
io.o: /usr/include/glib-2.0/glib/gerror.h /usr/include/glib-2.0/glib/gquark.h
io.o: /usr/include/glib-2.0/glib/gutils.h
io.o: /usr/include/glib-2.0/glib/gatomic.h
io.o: /usr/include/glib-2.0/glib/gbacktrace.h
io.o: /usr/include/glib-2.0/glib/gbase64.h
io.o: /usr/include/glib-2.0/glib/gbitlock.h
io.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
io.o: /usr/include/glib-2.0/glib/gcache.h /usr/include/glib-2.0/glib/glist.h
io.o: /usr/include/glib-2.0/glib/gmem.h /usr/include/glib-2.0/glib/gslice.h
io.o: /usr/include/glib-2.0/glib/gchecksum.h
io.o: /usr/include/glib-2.0/glib/gcompletion.h
io.o: /usr/include/glib-2.0/glib/gconvert.h
io.o: /usr/include/glib-2.0/glib/gdataset.h
io.o: /usr/include/glib-2.0/glib/gdate.h /usr/include/glib-2.0/glib/gdir.h
io.o: /usr/include/glib-2.0/glib/gfileutils.h
io.o: /usr/include/glib-2.0/glib/ghash.h /usr/include/glib-2.0/glib/ghook.h
io.o: /usr/include/glib-2.0/glib/ghostutils.h
io.o: /usr/include/glib-2.0/glib/giochannel.h
io.o: /usr/include/glib-2.0/glib/gmain.h /usr/include/glib-2.0/glib/gpoll.h
io.o: /usr/include/glib-2.0/glib/gslist.h
io.o: /usr/include/glib-2.0/glib/gstring.h
io.o: /usr/include/glib-2.0/glib/gunicode.h
io.o: /usr/include/glib-2.0/glib/gkeyfile.h
io.o: /usr/include/glib-2.0/glib/gmappedfile.h
io.o: /usr/include/glib-2.0/glib/gmarkup.h
io.o: /usr/include/glib-2.0/glib/gmessages.h
io.o: /usr/include/glib-2.0/glib/gnode.h /usr/include/glib-2.0/glib/goption.h
io.o: /usr/include/glib-2.0/glib/gpattern.h
io.o: /usr/include/glib-2.0/glib/gprimes.h
io.o: /usr/include/glib-2.0/glib/gqsort.h /usr/include/glib-2.0/glib/gqueue.h
io.o: /usr/include/glib-2.0/glib/grand.h /usr/include/glib-2.0/glib/grel.h
io.o: /usr/include/glib-2.0/glib/gregex.h
io.o: /usr/include/glib-2.0/glib/gscanner.h
io.o: /usr/include/glib-2.0/glib/gsequence.h
io.o: /usr/include/glib-2.0/glib/gshell.h /usr/include/glib-2.0/glib/gspawn.h
io.o: /usr/include/glib-2.0/glib/gstrfuncs.h
io.o: /usr/include/glib-2.0/glib/gtestutils.h
io.o: /usr/include/glib-2.0/glib/gthreadpool.h
io.o: /usr/include/glib-2.0/glib/gtimer.h /usr/include/glib-2.0/glib/gtree.h
io.o: /usr/include/glib-2.0/glib/gurifuncs.h
io.o: /usr/include/glib-2.0/glib/gvarianttype.h
io.o: /usr/include/glib-2.0/glib/gvariant.h aul/include/aul/mutex.h
io.o: aul/include/aul/common.h ./kernel.h aul/include/aul/log.h
io.o: aul/include/aul/exception.h aul/include/aul/string.h
io.o: aul/include/aul/list.h ./kernel-priv.h aul/include/aul/mainloop.h
io.o: aul/include/aul/hashtable.h ./buffer.h ./buffer2.h array.h
syscallblock.o: ./kernel.h aul/include/aul/common.h aul/include/aul/log.h
syscallblock.o: aul/include/aul/exception.h aul/include/aul/string.h
syscallblock.o: aul/include/aul/list.h ./kernel-priv.h
syscallblock.o: aul/include/aul/mainloop.h aul/include/aul/mutex.h
syscallblock.o: aul/include/aul/hashtable.h ./buffer.h ./buffer2.h
syscallblock.o: serialize.h
property.o: aul/include/aul/hashtable.h aul/include/aul/common.h
property.o: aul/include/aul/list.h ./kernel.h aul/include/aul/log.h
property.o: aul/include/aul/exception.h aul/include/aul/string.h
config.o: ./kernel.h aul/include/aul/common.h aul/include/aul/log.h
config.o: aul/include/aul/exception.h aul/include/aul/string.h
config.o: aul/include/aul/list.h ./kernel-priv.h aul/include/aul/mainloop.h
config.o: aul/include/aul/mutex.h aul/include/aul/hashtable.h
calibration.o: /usr/include/glib-2.0/glib.h
calibration.o: /usr/include/glib-2.0/glib/galloca.h
calibration.o: /usr/include/glib-2.0/glib/gtypes.h
calibration.o: /usr/lib/glib-2.0/include/glibconfig.h
calibration.o: /usr/include/glib-2.0/glib/gmacros.h
calibration.o: /usr/include/glib-2.0/glib/garray.h
calibration.o: /usr/include/glib-2.0/glib/gasyncqueue.h
calibration.o: /usr/include/glib-2.0/glib/gthread.h
calibration.o: /usr/include/glib-2.0/glib/gerror.h
calibration.o: /usr/include/glib-2.0/glib/gquark.h
calibration.o: /usr/include/glib-2.0/glib/gutils.h
calibration.o: /usr/include/glib-2.0/glib/gatomic.h
calibration.o: /usr/include/glib-2.0/glib/gbacktrace.h
calibration.o: /usr/include/glib-2.0/glib/gbase64.h
calibration.o: /usr/include/glib-2.0/glib/gbitlock.h
calibration.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
calibration.o: /usr/include/glib-2.0/glib/gcache.h
calibration.o: /usr/include/glib-2.0/glib/glist.h
calibration.o: /usr/include/glib-2.0/glib/gmem.h
calibration.o: /usr/include/glib-2.0/glib/gslice.h
calibration.o: /usr/include/glib-2.0/glib/gchecksum.h
calibration.o: /usr/include/glib-2.0/glib/gcompletion.h
calibration.o: /usr/include/glib-2.0/glib/gconvert.h
calibration.o: /usr/include/glib-2.0/glib/gdataset.h
calibration.o: /usr/include/glib-2.0/glib/gdate.h
calibration.o: /usr/include/glib-2.0/glib/gdir.h
calibration.o: /usr/include/glib-2.0/glib/gfileutils.h
calibration.o: /usr/include/glib-2.0/glib/ghash.h
calibration.o: /usr/include/glib-2.0/glib/ghook.h
calibration.o: /usr/include/glib-2.0/glib/ghostutils.h
calibration.o: /usr/include/glib-2.0/glib/giochannel.h
calibration.o: /usr/include/glib-2.0/glib/gmain.h
calibration.o: /usr/include/glib-2.0/glib/gpoll.h
calibration.o: /usr/include/glib-2.0/glib/gslist.h
calibration.o: /usr/include/glib-2.0/glib/gstring.h
calibration.o: /usr/include/glib-2.0/glib/gunicode.h
calibration.o: /usr/include/glib-2.0/glib/gkeyfile.h
calibration.o: /usr/include/glib-2.0/glib/gmappedfile.h
calibration.o: /usr/include/glib-2.0/glib/gmarkup.h
calibration.o: /usr/include/glib-2.0/glib/gmessages.h
calibration.o: /usr/include/glib-2.0/glib/gnode.h
calibration.o: /usr/include/glib-2.0/glib/goption.h
calibration.o: /usr/include/glib-2.0/glib/gpattern.h
calibration.o: /usr/include/glib-2.0/glib/gprimes.h
calibration.o: /usr/include/glib-2.0/glib/gqsort.h
calibration.o: /usr/include/glib-2.0/glib/gqueue.h
calibration.o: /usr/include/glib-2.0/glib/grand.h
calibration.o: /usr/include/glib-2.0/glib/grel.h
calibration.o: /usr/include/glib-2.0/glib/gregex.h
calibration.o: /usr/include/glib-2.0/glib/gscanner.h
calibration.o: /usr/include/glib-2.0/glib/gsequence.h
calibration.o: /usr/include/glib-2.0/glib/gshell.h
calibration.o: /usr/include/glib-2.0/glib/gspawn.h
calibration.o: /usr/include/glib-2.0/glib/gstrfuncs.h
calibration.o: /usr/include/glib-2.0/glib/gtestutils.h
calibration.o: /usr/include/glib-2.0/glib/gthreadpool.h
calibration.o: /usr/include/glib-2.0/glib/gtimer.h
calibration.o: /usr/include/glib-2.0/glib/gtree.h
calibration.o: /usr/include/glib-2.0/glib/gurifuncs.h
calibration.o: /usr/include/glib-2.0/glib/gvarianttype.h
calibration.o: /usr/include/glib-2.0/glib/gvariant.h aul/include/aul/common.h
calibration.o: ./kernel.h aul/include/aul/log.h aul/include/aul/exception.h
calibration.o: aul/include/aul/string.h aul/include/aul/list.h
calibration.o: ./kernel-priv.h aul/include/aul/mainloop.h
calibration.o: aul/include/aul/mutex.h aul/include/aul/hashtable.h
buffer2.o: ./kernel.h aul/include/aul/common.h aul/include/aul/log.h
buffer2.o: aul/include/aul/exception.h aul/include/aul/string.h
buffer2.o: aul/include/aul/list.h ./kernel-priv.h aul/include/aul/mainloop.h
buffer2.o: aul/include/aul/mutex.h aul/include/aul/hashtable.h ./buffer.h
buffer2.o: ./buffer2.h
serialize.o: serialize.h ./buffer.h ./buffer2.h ./kernel.h
serialize.o: aul/include/aul/common.h aul/include/aul/log.h
serialize.o: aul/include/aul/exception.h aul/include/aul/string.h
serialize.o: aul/include/aul/list.h ./kernel-priv.h
serialize.o: aul/include/aul/mainloop.h aul/include/aul/mutex.h
serialize.o: aul/include/aul/hashtable.h
trigger.o: ./kernel.h aul/include/aul/common.h aul/include/aul/log.h
trigger.o: aul/include/aul/exception.h aul/include/aul/string.h
trigger.o: aul/include/aul/list.h ./kernel-priv.h aul/include/aul/mainloop.h
trigger.o: aul/include/aul/mutex.h aul/include/aul/hashtable.h
exec.o: ./kernel.h aul/include/aul/common.h aul/include/aul/log.h
exec.o: aul/include/aul/exception.h aul/include/aul/string.h
exec.o: aul/include/aul/list.h ./kernel-priv.h aul/include/aul/mainloop.h
exec.o: aul/include/aul/mutex.h aul/include/aul/hashtable.h
luaenv.o: /usr/include/lua5.1/lua.h /usr/include/lua5.1/luaconf.h
luaenv.o: /usr/include/lua5.1/lauxlib.h /usr/include/lua5.1/lualib.h
luaenv.o: ./kernel.h aul/include/aul/common.h aul/include/aul/log.h
luaenv.o: aul/include/aul/exception.h aul/include/aul/string.h
luaenv.o: aul/include/aul/list.h ./kernel-priv.h aul/include/aul/mainloop.h
luaenv.o: aul/include/aul/mutex.h aul/include/aul/hashtable.h
math.o: /usr/include/glib-2.0/glib.h /usr/include/glib-2.0/glib/galloca.h
math.o: /usr/include/glib-2.0/glib/gtypes.h
math.o: /usr/lib/glib-2.0/include/glibconfig.h
math.o: /usr/include/glib-2.0/glib/gmacros.h
math.o: /usr/include/glib-2.0/glib/garray.h
math.o: /usr/include/glib-2.0/glib/gasyncqueue.h
math.o: /usr/include/glib-2.0/glib/gthread.h
math.o: /usr/include/glib-2.0/glib/gerror.h
math.o: /usr/include/glib-2.0/glib/gquark.h
math.o: /usr/include/glib-2.0/glib/gutils.h
math.o: /usr/include/glib-2.0/glib/gatomic.h
math.o: /usr/include/glib-2.0/glib/gbacktrace.h
math.o: /usr/include/glib-2.0/glib/gbase64.h
math.o: /usr/include/glib-2.0/glib/gbitlock.h
math.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
math.o: /usr/include/glib-2.0/glib/gcache.h
math.o: /usr/include/glib-2.0/glib/glist.h /usr/include/glib-2.0/glib/gmem.h
math.o: /usr/include/glib-2.0/glib/gslice.h
math.o: /usr/include/glib-2.0/glib/gchecksum.h
math.o: /usr/include/glib-2.0/glib/gcompletion.h
math.o: /usr/include/glib-2.0/glib/gconvert.h
math.o: /usr/include/glib-2.0/glib/gdataset.h
math.o: /usr/include/glib-2.0/glib/gdate.h /usr/include/glib-2.0/glib/gdir.h
math.o: /usr/include/glib-2.0/glib/gfileutils.h
math.o: /usr/include/glib-2.0/glib/ghash.h /usr/include/glib-2.0/glib/ghook.h
math.o: /usr/include/glib-2.0/glib/ghostutils.h
math.o: /usr/include/glib-2.0/glib/giochannel.h
math.o: /usr/include/glib-2.0/glib/gmain.h /usr/include/glib-2.0/glib/gpoll.h
math.o: /usr/include/glib-2.0/glib/gslist.h
math.o: /usr/include/glib-2.0/glib/gstring.h
math.o: /usr/include/glib-2.0/glib/gunicode.h
math.o: /usr/include/glib-2.0/glib/gkeyfile.h
math.o: /usr/include/glib-2.0/glib/gmappedfile.h
math.o: /usr/include/glib-2.0/glib/gmarkup.h
math.o: /usr/include/glib-2.0/glib/gmessages.h
math.o: /usr/include/glib-2.0/glib/gnode.h
math.o: /usr/include/glib-2.0/glib/goption.h
math.o: /usr/include/glib-2.0/glib/gpattern.h
math.o: /usr/include/glib-2.0/glib/gprimes.h
math.o: /usr/include/glib-2.0/glib/gqsort.h
math.o: /usr/include/glib-2.0/glib/gqueue.h
math.o: /usr/include/glib-2.0/glib/grand.h /usr/include/glib-2.0/glib/grel.h
math.o: /usr/include/glib-2.0/glib/gregex.h
math.o: /usr/include/glib-2.0/glib/gscanner.h
math.o: /usr/include/glib-2.0/glib/gsequence.h
math.o: /usr/include/glib-2.0/glib/gshell.h
math.o: /usr/include/glib-2.0/glib/gspawn.h
math.o: /usr/include/glib-2.0/glib/gstrfuncs.h
math.o: /usr/include/glib-2.0/glib/gtestutils.h
math.o: /usr/include/glib-2.0/glib/gthreadpool.h
math.o: /usr/include/glib-2.0/glib/gtimer.h
math.o: /usr/include/glib-2.0/glib/gtree.h
math.o: /usr/include/glib-2.0/glib/gurifuncs.h
math.o: /usr/include/glib-2.0/glib/gvarianttype.h
math.o: /usr/include/glib-2.0/glib/gvariant.h ./kernel.h
math.o: aul/include/aul/common.h aul/include/aul/log.h
math.o: aul/include/aul/exception.h aul/include/aul/string.h
math.o: aul/include/aul/list.h ./kernel-priv.h aul/include/aul/mainloop.h
math.o: aul/include/aul/mutex.h aul/include/aul/hashtable.h
