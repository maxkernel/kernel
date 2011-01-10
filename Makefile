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

MODULES		= netui discovery console service httpserver network map maxpod jpegcompress webcam gps test
HEADERS		= kernel.h buffer.h array.h serialize.h 

SRCS		= kernel.c meta.c module.c profile.c memfs.c syscall.c io.c syscallblock.c property.c config.c calibration.c buffer.c serialize.c trigger.c exec.c luaenv.c math.c
OBJS		= $(SRCS:.c=.o)
PACKAGES	= libconfuse libffi glib-2.0 gnet-2.0 gmodule-2.0 sqlite3 lua5.1 libmatheval
INCLUDES	= -I. -Iaul/include $(shell pkg-config --cflags-only-I $(PACKAGES))
DEFINES		= -D_GNU_SOURCE -DKERNEL $(shell [ "$(PROFILE)" = 'yes' ] && echo "-DEN_PROFILE" ) -D$(RELEASE) -DRELEASE="\"$(RELEASE)\"" -DINSTALL="\"$(INSTALL)\"" -DLOGDIR="\"$(LOGDIR)\"" -DDBNAME="\"$(DBNAME)\"" -DCONFIG="\"$(CONFIG)\"" -DMEMFS="\"$(MEMFS)\"" -DMODEL="\"$(MODEL)\""
CFLAGS		= -pipe -ggdb3 -Wall $(shell pkg-config --cflags-only-other $(PACKAGES))
LIBS		= $(shell pkg-config --libs $(PACKAGES)) -lbfd -ldl -lrt -laul -Laul
LFLAGS		= 

TARGET		= maxkernel

#OBJECTS		= kernel.o meta.o module.o profile.o syscall.o io.o syscallblock.o property.o config.o calibration.o buffer.o serialize.o trigger.o exec.o luaenv.o math.o
#HEADERS		= kernel.h buffer.h array.h serialize.h 
#TARGET		= maxkernel
#MODULES		= netui discovery console service httpserver network map jpegcompress webcam test
# Need to fix = maxpod
# IN DEVEL MOD = x264compress nimu pololu-mssc videre miscserver
#UTILS		= max-kdump max-modinfo max-syscall max-log max-autostart max-client
#PACKAGES	= libconfuse libffi glib-2.0 gnet-2.0 gmodule-2.0 sqlite lua5.1 libmatheval
#DEFINES		= -D_GNU_SOURCE -DKERNEL $(shell [ "$(PROFILE)" = 'yes' ] && echo "-DEN_PROFILE" ) -D$(RELEASE) -DRELEASE="\"$(RELEASE)\"" -DINSTALL="\"$(INSTALL)\"" -DLOGDIR="\"$(LOGDIR)\"" -DDBNAME="\"$(DBNAME)\"" -DCONFIG="\"$(CONFIG)\"" -DMODEL="\"$(MODEL)\""

#LIBS		= $(shell pkg-config --libs $(PACKAGES)) -lbfd -ldl -lrt -laul -Laul
#CFLAGS		= -pipe -ggdb3 -Wall $(DEFINES) -I. -Iaul/include $(shell pkg-config --cflags $(PACKAGES))

export INSTALL
export RELEASE

.PHONY: prepare prereq install clean rebuild depend

all: prepare prereq $(TARGET)
	$(foreach module,$(MODULES),perl makefile.gen.pl -module $(module) -defines '$(DEFINES)' >$(module)/Makefile && $(MAKE) -C $(module) depend 2>/dev/null &&) true
	( $(foreach module,$(MODULES), echo "In module $(module)" >>buildlog && $(MAKE) -C $(module) 2>>buildlog &&) true ) || ( cat buildlog && false )
	( echo "In libmax" >>buildlog && $(MAKE) -C libmax 2>>buildlog ) || ( cat buildlog && false )
	( echo "In utils" >>buildlog && $(MAKE) -C utils 2>>buildlog) || ( cat buildlog && false )
	( echo "In gendb" >>buildlog && cat database.gen.sql | sqlite3 $(DBNAME) >>buildlog ) || ( cat buildlog && false )
	cat buildlog

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

install:
	mkdir -p $(INSTALL) $(LOGDIR) $(INSTALL)/modules $(INSTALL)/deploy /usr/include/max
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
	rm -f $(TARGET) $(OBJS) $(foreach module,$(MODULES),$(module)/Makefile)

rebuild: clean all

depend: $(SRCS)
	makedepend $(DEFINES) $(INCLUDES) $^
	$(MAKE) -C aul depend
	$(MAKE) -C libmax depend

prepare:
	echo "Build Log ==----------------------== [$(shell date)]" >buildlog

prereq:
	( echo "In aul" >>buildlog && $(MAKE) -C aul 2>>buildlog ) || ( cat buildlog && false )
	echo "In kernel" >>buildlog

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

kernel.o: /usr/include/stdlib.h /usr/include/features.h
kernel.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
kernel.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
kernel.o: /usr/include/gnu/stubs-64.h /usr/include/bits/waitflags.h
kernel.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
kernel.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
kernel.o: /usr/include/xlocale.h /usr/include/sys/types.h
kernel.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
kernel.o: /usr/include/time.h /usr/include/sys/select.h
kernel.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
kernel.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
kernel.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
kernel.o: /usr/include/stdio.h /usr/include/libio.h /usr/include/_G_config.h
kernel.o: /usr/include/wchar.h /usr/include/bits/stdio_lim.h
kernel.o: /usr/include/bits/sys_errlist.h /usr/include/unistd.h
kernel.o: /usr/include/bits/posix_opt.h /usr/include/bits/environments.h
kernel.o: /usr/include/bits/confname.h /usr/include/getopt.h
kernel.o: /usr/include/string.h /usr/include/argp.h /usr/include/ctype.h
kernel.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
kernel.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
kernel.o: /usr/include/bits/posix2_lim.h /usr/include/bits/xopen_lim.h
kernel.o: /usr/include/errno.h /usr/include/bits/errno.h
kernel.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
kernel.o: /usr/include/asm-generic/errno.h
kernel.o: /usr/include/asm-generic/errno-base.h /usr/include/malloc.h
kernel.o: /usr/include/dirent.h /usr/include/bits/dirent.h
kernel.o: /usr/include/sys/mman.h /usr/include/bits/mman.h
kernel.o: /usr/include/sys/time.h /usr/include/pthread.h /usr/include/sched.h
kernel.o: /usr/include/bits/sched.h /usr/include/signal.h
kernel.o: /usr/include/bits/setjmp.h /usr/include/bfd.h
kernel.o: /usr/include/ansidecl.h /usr/include/symcat.h
kernel.o: /usr/include/confuse.h /usr/include/sqlite3.h
kernel.o: aul/include/aul/common.h /usr/include/inttypes.h
kernel.o: /usr/include/stdint.h /usr/include/bits/wchar.h
kernel.o: aul/include/aul/list.h aul/include/aul/mainloop.h
kernel.o: aul/include/aul/mutex.h kernel.h /usr/include/glib-2.0/glib.h
kernel.o: /usr/include/glib-2.0/glib/galloca.h
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
kernel.o: /usr/include/glib-2.0/glib/gvariant.h
kernel.o: /usr/include/glib-2.0/gmodule.h aul/include/aul/log.h
kernel.o: aul/include/aul/error.h aul/include/aul/string.h kernel-priv.h
meta.o: /usr/include/string.h /usr/include/features.h
meta.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
meta.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
meta.o: /usr/include/gnu/stubs-64.h /usr/include/xlocale.h
meta.o: /usr/include/ctype.h /usr/include/bits/types.h
meta.o: /usr/include/bits/typesizes.h /usr/include/endian.h
meta.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
meta.o: /usr/include/sys/stat.h /usr/include/time.h /usr/include/bits/stat.h
meta.o: /usr/include/bfd.h /usr/include/ansidecl.h /usr/include/symcat.h
meta.o: kernel.h /usr/include/stdlib.h /usr/include/bits/waitflags.h
meta.o: /usr/include/bits/waitstatus.h /usr/include/sys/types.h
meta.o: /usr/include/sys/select.h /usr/include/bits/select.h
meta.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
meta.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
meta.o: /usr/include/alloca.h /usr/include/inttypes.h /usr/include/stdint.h
meta.o: /usr/include/bits/wchar.h /usr/include/glib-2.0/glib.h
meta.o: /usr/include/glib-2.0/glib/galloca.h
meta.o: /usr/include/glib-2.0/glib/gtypes.h
meta.o: /usr/lib/glib-2.0/include/glibconfig.h
meta.o: /usr/include/glib-2.0/glib/gmacros.h /usr/include/limits.h
meta.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
meta.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
meta.o: /usr/include/bits/xopen_lim.h /usr/include/bits/stdio_lim.h
meta.o: /usr/include/glib-2.0/glib/garray.h
meta.o: /usr/include/glib-2.0/glib/gasyncqueue.h
meta.o: /usr/include/glib-2.0/glib/gthread.h
meta.o: /usr/include/glib-2.0/glib/gerror.h
meta.o: /usr/include/glib-2.0/glib/gquark.h
meta.o: /usr/include/glib-2.0/glib/gutils.h
meta.o: /usr/include/glib-2.0/glib/gatomic.h
meta.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
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
meta.o: /usr/include/glib-2.0/glib/gvariant.h /usr/include/glib-2.0/gmodule.h
meta.o: aul/include/aul/common.h aul/include/aul/log.h
meta.o: aul/include/aul/error.h aul/include/aul/string.h
meta.o: aul/include/aul/list.h kernel-priv.h /usr/include/sched.h
meta.o: /usr/include/bits/sched.h
module.o: /usr/include/stdlib.h /usr/include/features.h
module.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
module.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
module.o: /usr/include/gnu/stubs-64.h /usr/include/bits/waitflags.h
module.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
module.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
module.o: /usr/include/xlocale.h /usr/include/sys/types.h
module.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
module.o: /usr/include/time.h /usr/include/sys/select.h
module.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
module.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
module.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
module.o: /usr/include/string.h /usr/include/ctype.h /usr/include/dlfcn.h
module.o: /usr/include/bits/dlfcn.h kernel.h /usr/include/inttypes.h
module.o: /usr/include/stdint.h /usr/include/bits/wchar.h
module.o: /usr/include/glib-2.0/glib.h /usr/include/glib-2.0/glib/galloca.h
module.o: /usr/include/glib-2.0/glib/gtypes.h
module.o: /usr/lib/glib-2.0/include/glibconfig.h
module.o: /usr/include/glib-2.0/glib/gmacros.h /usr/include/limits.h
module.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
module.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
module.o: /usr/include/bits/xopen_lim.h /usr/include/bits/stdio_lim.h
module.o: /usr/include/glib-2.0/glib/garray.h
module.o: /usr/include/glib-2.0/glib/gasyncqueue.h
module.o: /usr/include/glib-2.0/glib/gthread.h
module.o: /usr/include/glib-2.0/glib/gerror.h
module.o: /usr/include/glib-2.0/glib/gquark.h
module.o: /usr/include/glib-2.0/glib/gutils.h
module.o: /usr/include/glib-2.0/glib/gatomic.h
module.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
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
module.o: /usr/include/glib-2.0/glib/gvariant.h
module.o: /usr/include/glib-2.0/gmodule.h aul/include/aul/common.h
module.o: aul/include/aul/log.h aul/include/aul/error.h
module.o: aul/include/aul/string.h aul/include/aul/list.h kernel-priv.h
module.o: /usr/include/sched.h /usr/include/bits/sched.h
profile.o: /usr/include/inttypes.h /usr/include/features.h
profile.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
profile.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
profile.o: /usr/include/gnu/stubs-64.h /usr/include/stdint.h
profile.o: /usr/include/bits/wchar.h kernel.h /usr/include/stdlib.h
profile.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
profile.o: /usr/include/endian.h /usr/include/bits/endian.h
profile.o: /usr/include/bits/byteswap.h /usr/include/xlocale.h
profile.o: /usr/include/sys/types.h /usr/include/bits/types.h
profile.o: /usr/include/bits/typesizes.h /usr/include/time.h
profile.o: /usr/include/sys/select.h /usr/include/bits/select.h
profile.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
profile.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
profile.o: /usr/include/alloca.h /usr/include/glib-2.0/glib.h
profile.o: /usr/include/glib-2.0/glib/galloca.h
profile.o: /usr/include/glib-2.0/glib/gtypes.h
profile.o: /usr/lib/glib-2.0/include/glibconfig.h
profile.o: /usr/include/glib-2.0/glib/gmacros.h /usr/include/limits.h
profile.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
profile.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
profile.o: /usr/include/bits/xopen_lim.h /usr/include/bits/stdio_lim.h
profile.o: /usr/include/glib-2.0/glib/garray.h
profile.o: /usr/include/glib-2.0/glib/gasyncqueue.h
profile.o: /usr/include/glib-2.0/glib/gthread.h
profile.o: /usr/include/glib-2.0/glib/gerror.h
profile.o: /usr/include/glib-2.0/glib/gquark.h
profile.o: /usr/include/glib-2.0/glib/gutils.h
profile.o: /usr/include/glib-2.0/glib/gatomic.h
profile.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
profile.o: /usr/include/glib-2.0/glib/gbase64.h
profile.o: /usr/include/glib-2.0/glib/gbitlock.h
profile.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
profile.o: /usr/include/glib-2.0/glib/gcache.h
profile.o: /usr/include/glib-2.0/glib/glist.h
profile.o: /usr/include/glib-2.0/glib/gmem.h
profile.o: /usr/include/glib-2.0/glib/gslice.h
profile.o: /usr/include/glib-2.0/glib/gchecksum.h
profile.o: /usr/include/glib-2.0/glib/gcompletion.h
profile.o: /usr/include/glib-2.0/glib/gconvert.h
profile.o: /usr/include/glib-2.0/glib/gdataset.h
profile.o: /usr/include/glib-2.0/glib/gdate.h
profile.o: /usr/include/glib-2.0/glib/gdir.h
profile.o: /usr/include/glib-2.0/glib/gfileutils.h
profile.o: /usr/include/glib-2.0/glib/ghash.h
profile.o: /usr/include/glib-2.0/glib/ghook.h
profile.o: /usr/include/glib-2.0/glib/ghostutils.h
profile.o: /usr/include/glib-2.0/glib/giochannel.h
profile.o: /usr/include/glib-2.0/glib/gmain.h
profile.o: /usr/include/glib-2.0/glib/gpoll.h
profile.o: /usr/include/glib-2.0/glib/gslist.h
profile.o: /usr/include/glib-2.0/glib/gstring.h
profile.o: /usr/include/glib-2.0/glib/gunicode.h
profile.o: /usr/include/glib-2.0/glib/gkeyfile.h
profile.o: /usr/include/glib-2.0/glib/gmappedfile.h
profile.o: /usr/include/glib-2.0/glib/gmarkup.h
profile.o: /usr/include/glib-2.0/glib/gmessages.h
profile.o: /usr/include/glib-2.0/glib/gnode.h
profile.o: /usr/include/glib-2.0/glib/goption.h
profile.o: /usr/include/glib-2.0/glib/gpattern.h
profile.o: /usr/include/glib-2.0/glib/gprimes.h
profile.o: /usr/include/glib-2.0/glib/gqsort.h
profile.o: /usr/include/glib-2.0/glib/gqueue.h
profile.o: /usr/include/glib-2.0/glib/grand.h
profile.o: /usr/include/glib-2.0/glib/grel.h
profile.o: /usr/include/glib-2.0/glib/gregex.h
profile.o: /usr/include/glib-2.0/glib/gscanner.h
profile.o: /usr/include/glib-2.0/glib/gsequence.h
profile.o: /usr/include/glib-2.0/glib/gshell.h
profile.o: /usr/include/glib-2.0/glib/gspawn.h
profile.o: /usr/include/glib-2.0/glib/gstrfuncs.h
profile.o: /usr/include/glib-2.0/glib/gtestutils.h
profile.o: /usr/include/glib-2.0/glib/gthreadpool.h
profile.o: /usr/include/glib-2.0/glib/gtimer.h
profile.o: /usr/include/glib-2.0/glib/gtree.h
profile.o: /usr/include/glib-2.0/glib/gurifuncs.h
profile.o: /usr/include/glib-2.0/glib/gvarianttype.h
profile.o: /usr/include/glib-2.0/glib/gvariant.h
profile.o: /usr/include/glib-2.0/gmodule.h aul/include/aul/common.h
profile.o: /usr/include/string.h aul/include/aul/log.h
profile.o: aul/include/aul/error.h aul/include/aul/string.h
profile.o: aul/include/aul/list.h kernel-priv.h /usr/include/sched.h
profile.o: /usr/include/bits/sched.h
memfs.o: /usr/include/unistd.h /usr/include/features.h
memfs.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
memfs.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
memfs.o: /usr/include/gnu/stubs-64.h /usr/include/bits/posix_opt.h
memfs.o: /usr/include/bits/environments.h /usr/include/bits/types.h
memfs.o: /usr/include/bits/typesizes.h /usr/include/bits/confname.h
memfs.o: /usr/include/getopt.h /usr/include/errno.h /usr/include/bits/errno.h
memfs.o: /usr/include/linux/errno.h /usr/include/asm/errno.h
memfs.o: /usr/include/asm-generic/errno.h
memfs.o: /usr/include/asm-generic/errno-base.h /usr/include/fcntl.h
memfs.o: /usr/include/bits/fcntl.h /usr/include/sys/types.h
memfs.o: /usr/include/time.h /usr/include/endian.h /usr/include/bits/endian.h
memfs.o: /usr/include/bits/byteswap.h /usr/include/sys/select.h
memfs.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
memfs.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
memfs.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/uio.h
memfs.o: /usr/include/sys/stat.h /usr/include/bits/stat.h
memfs.o: /usr/include/stdlib.h /usr/include/bits/waitflags.h
memfs.o: /usr/include/bits/waitstatus.h /usr/include/xlocale.h
memfs.o: /usr/include/alloca.h /usr/include/sys/mount.h
memfs.o: /usr/include/sys/ioctl.h /usr/include/bits/ioctls.h
memfs.o: /usr/include/asm/ioctls.h /usr/include/asm-generic/ioctls.h
memfs.o: /usr/include/linux/ioctl.h /usr/include/asm/ioctl.h
memfs.o: /usr/include/asm-generic/ioctl.h /usr/include/bits/ioctl-types.h
memfs.o: /usr/include/sys/ttydefaults.h aul/include/aul/string.h
memfs.o: /usr/include/string.h aul/include/aul/common.h
memfs.o: /usr/include/inttypes.h /usr/include/stdint.h
memfs.o: /usr/include/bits/wchar.h aul/include/aul/error.h kernel.h
memfs.o: /usr/include/glib-2.0/glib.h /usr/include/glib-2.0/glib/galloca.h
memfs.o: /usr/include/glib-2.0/glib/gtypes.h
memfs.o: /usr/lib/glib-2.0/include/glibconfig.h
memfs.o: /usr/include/glib-2.0/glib/gmacros.h /usr/include/limits.h
memfs.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
memfs.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
memfs.o: /usr/include/bits/xopen_lim.h /usr/include/bits/stdio_lim.h
memfs.o: /usr/include/glib-2.0/glib/garray.h
memfs.o: /usr/include/glib-2.0/glib/gasyncqueue.h
memfs.o: /usr/include/glib-2.0/glib/gthread.h
memfs.o: /usr/include/glib-2.0/glib/gerror.h
memfs.o: /usr/include/glib-2.0/glib/gquark.h
memfs.o: /usr/include/glib-2.0/glib/gutils.h
memfs.o: /usr/include/glib-2.0/glib/gatomic.h
memfs.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
memfs.o: /usr/include/glib-2.0/glib/gbase64.h
memfs.o: /usr/include/glib-2.0/glib/gbitlock.h
memfs.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
memfs.o: /usr/include/glib-2.0/glib/gcache.h
memfs.o: /usr/include/glib-2.0/glib/glist.h /usr/include/glib-2.0/glib/gmem.h
memfs.o: /usr/include/glib-2.0/glib/gslice.h
memfs.o: /usr/include/glib-2.0/glib/gchecksum.h
memfs.o: /usr/include/glib-2.0/glib/gcompletion.h
memfs.o: /usr/include/glib-2.0/glib/gconvert.h
memfs.o: /usr/include/glib-2.0/glib/gdataset.h
memfs.o: /usr/include/glib-2.0/glib/gdate.h /usr/include/glib-2.0/glib/gdir.h
memfs.o: /usr/include/glib-2.0/glib/gfileutils.h
memfs.o: /usr/include/glib-2.0/glib/ghash.h
memfs.o: /usr/include/glib-2.0/glib/ghook.h
memfs.o: /usr/include/glib-2.0/glib/ghostutils.h
memfs.o: /usr/include/glib-2.0/glib/giochannel.h
memfs.o: /usr/include/glib-2.0/glib/gmain.h
memfs.o: /usr/include/glib-2.0/glib/gpoll.h
memfs.o: /usr/include/glib-2.0/glib/gslist.h
memfs.o: /usr/include/glib-2.0/glib/gstring.h
memfs.o: /usr/include/glib-2.0/glib/gunicode.h
memfs.o: /usr/include/glib-2.0/glib/gkeyfile.h
memfs.o: /usr/include/glib-2.0/glib/gmappedfile.h
memfs.o: /usr/include/glib-2.0/glib/gmarkup.h
memfs.o: /usr/include/glib-2.0/glib/gmessages.h
memfs.o: /usr/include/glib-2.0/glib/gnode.h
memfs.o: /usr/include/glib-2.0/glib/goption.h
memfs.o: /usr/include/glib-2.0/glib/gpattern.h
memfs.o: /usr/include/glib-2.0/glib/gprimes.h
memfs.o: /usr/include/glib-2.0/glib/gqsort.h
memfs.o: /usr/include/glib-2.0/glib/gqueue.h
memfs.o: /usr/include/glib-2.0/glib/grand.h /usr/include/glib-2.0/glib/grel.h
memfs.o: /usr/include/glib-2.0/glib/gregex.h
memfs.o: /usr/include/glib-2.0/glib/gscanner.h
memfs.o: /usr/include/glib-2.0/glib/gsequence.h
memfs.o: /usr/include/glib-2.0/glib/gshell.h
memfs.o: /usr/include/glib-2.0/glib/gspawn.h
memfs.o: /usr/include/glib-2.0/glib/gstrfuncs.h
memfs.o: /usr/include/glib-2.0/glib/gtestutils.h
memfs.o: /usr/include/glib-2.0/glib/gthreadpool.h
memfs.o: /usr/include/glib-2.0/glib/gtimer.h
memfs.o: /usr/include/glib-2.0/glib/gtree.h
memfs.o: /usr/include/glib-2.0/glib/gurifuncs.h
memfs.o: /usr/include/glib-2.0/glib/gvarianttype.h
memfs.o: /usr/include/glib-2.0/glib/gvariant.h
memfs.o: /usr/include/glib-2.0/gmodule.h aul/include/aul/log.h
memfs.o: aul/include/aul/list.h kernel-priv.h /usr/include/sched.h
memfs.o: /usr/include/bits/sched.h
syscall.o: /usr/include/string.h /usr/include/features.h
syscall.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
syscall.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
syscall.o: /usr/include/gnu/stubs-64.h /usr/include/xlocale.h kernel.h
syscall.o: /usr/include/stdlib.h /usr/include/bits/waitflags.h
syscall.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
syscall.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
syscall.o: /usr/include/sys/types.h /usr/include/bits/types.h
syscall.o: /usr/include/bits/typesizes.h /usr/include/time.h
syscall.o: /usr/include/sys/select.h /usr/include/bits/select.h
syscall.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
syscall.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
syscall.o: /usr/include/alloca.h /usr/include/inttypes.h
syscall.o: /usr/include/stdint.h /usr/include/bits/wchar.h
syscall.o: /usr/include/glib-2.0/glib.h /usr/include/glib-2.0/glib/galloca.h
syscall.o: /usr/include/glib-2.0/glib/gtypes.h
syscall.o: /usr/lib/glib-2.0/include/glibconfig.h
syscall.o: /usr/include/glib-2.0/glib/gmacros.h /usr/include/limits.h
syscall.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
syscall.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
syscall.o: /usr/include/bits/xopen_lim.h /usr/include/bits/stdio_lim.h
syscall.o: /usr/include/glib-2.0/glib/garray.h
syscall.o: /usr/include/glib-2.0/glib/gasyncqueue.h
syscall.o: /usr/include/glib-2.0/glib/gthread.h
syscall.o: /usr/include/glib-2.0/glib/gerror.h
syscall.o: /usr/include/glib-2.0/glib/gquark.h
syscall.o: /usr/include/glib-2.0/glib/gutils.h
syscall.o: /usr/include/glib-2.0/glib/gatomic.h
syscall.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
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
syscall.o: /usr/include/glib-2.0/glib/gvariant.h
syscall.o: /usr/include/glib-2.0/gmodule.h aul/include/aul/common.h
syscall.o: aul/include/aul/log.h aul/include/aul/error.h
syscall.o: aul/include/aul/string.h aul/include/aul/list.h kernel-priv.h
syscall.o: /usr/include/sched.h /usr/include/bits/sched.h array.h ./buffer.h
syscall.o: serialize.h
io.o: /usr/include/string.h /usr/include/features.h
io.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
io.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
io.o: /usr/include/gnu/stubs-64.h /usr/include/xlocale.h /usr/include/ctype.h
io.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
io.o: /usr/include/endian.h /usr/include/bits/endian.h
io.o: /usr/include/bits/byteswap.h /usr/include/glib-2.0/glib.h
io.o: /usr/include/glib-2.0/glib/galloca.h
io.o: /usr/include/glib-2.0/glib/gtypes.h
io.o: /usr/lib/glib-2.0/include/glibconfig.h
io.o: /usr/include/glib-2.0/glib/gmacros.h /usr/include/limits.h
io.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
io.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
io.o: /usr/include/bits/xopen_lim.h /usr/include/bits/stdio_lim.h
io.o: /usr/include/glib-2.0/glib/garray.h
io.o: /usr/include/glib-2.0/glib/gasyncqueue.h
io.o: /usr/include/glib-2.0/glib/gthread.h
io.o: /usr/include/glib-2.0/glib/gerror.h /usr/include/glib-2.0/glib/gquark.h
io.o: /usr/include/glib-2.0/glib/gutils.h
io.o: /usr/include/glib-2.0/glib/gatomic.h
io.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
io.o: /usr/include/bits/sigset.h /usr/include/glib-2.0/glib/gbase64.h
io.o: /usr/include/glib-2.0/glib/gbitlock.h
io.o: /usr/include/glib-2.0/glib/gbookmarkfile.h /usr/include/time.h
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
io.o: /usr/include/pthread.h /usr/include/sched.h /usr/include/bits/sched.h
io.o: /usr/include/bits/pthreadtypes.h /usr/include/bits/setjmp.h
io.o: aul/include/aul/common.h /usr/include/stdlib.h
io.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
io.o: /usr/include/sys/types.h /usr/include/sys/select.h
io.o: /usr/include/bits/select.h /usr/include/bits/time.h
io.o: /usr/include/sys/sysmacros.h /usr/include/alloca.h
io.o: /usr/include/inttypes.h /usr/include/stdint.h /usr/include/bits/wchar.h
io.o: kernel.h /usr/include/glib-2.0/gmodule.h aul/include/aul/log.h
io.o: aul/include/aul/error.h aul/include/aul/string.h aul/include/aul/list.h
io.o: kernel-priv.h ./buffer.h array.h
syscallblock.o: kernel.h /usr/include/stdlib.h /usr/include/features.h
syscallblock.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
syscallblock.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
syscallblock.o: /usr/include/gnu/stubs-64.h /usr/include/bits/waitflags.h
syscallblock.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
syscallblock.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
syscallblock.o: /usr/include/xlocale.h /usr/include/sys/types.h
syscallblock.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
syscallblock.o: /usr/include/time.h /usr/include/sys/select.h
syscallblock.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
syscallblock.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
syscallblock.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
syscallblock.o: /usr/include/inttypes.h /usr/include/stdint.h
syscallblock.o: /usr/include/bits/wchar.h /usr/include/glib-2.0/glib.h
syscallblock.o: /usr/include/glib-2.0/glib/galloca.h
syscallblock.o: /usr/include/glib-2.0/glib/gtypes.h
syscallblock.o: /usr/lib/glib-2.0/include/glibconfig.h
syscallblock.o: /usr/include/glib-2.0/glib/gmacros.h /usr/include/limits.h
syscallblock.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
syscallblock.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
syscallblock.o: /usr/include/bits/xopen_lim.h /usr/include/bits/stdio_lim.h
syscallblock.o: /usr/include/glib-2.0/glib/garray.h
syscallblock.o: /usr/include/glib-2.0/glib/gasyncqueue.h
syscallblock.o: /usr/include/glib-2.0/glib/gthread.h
syscallblock.o: /usr/include/glib-2.0/glib/gerror.h
syscallblock.o: /usr/include/glib-2.0/glib/gquark.h
syscallblock.o: /usr/include/glib-2.0/glib/gutils.h
syscallblock.o: /usr/include/glib-2.0/glib/gatomic.h
syscallblock.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
syscallblock.o: /usr/include/glib-2.0/glib/gbase64.h
syscallblock.o: /usr/include/glib-2.0/glib/gbitlock.h
syscallblock.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
syscallblock.o: /usr/include/glib-2.0/glib/gcache.h
syscallblock.o: /usr/include/glib-2.0/glib/glist.h
syscallblock.o: /usr/include/glib-2.0/glib/gmem.h
syscallblock.o: /usr/include/glib-2.0/glib/gslice.h
syscallblock.o: /usr/include/glib-2.0/glib/gchecksum.h
syscallblock.o: /usr/include/glib-2.0/glib/gcompletion.h
syscallblock.o: /usr/include/glib-2.0/glib/gconvert.h
syscallblock.o: /usr/include/glib-2.0/glib/gdataset.h
syscallblock.o: /usr/include/glib-2.0/glib/gdate.h
syscallblock.o: /usr/include/glib-2.0/glib/gdir.h
syscallblock.o: /usr/include/glib-2.0/glib/gfileutils.h
syscallblock.o: /usr/include/glib-2.0/glib/ghash.h
syscallblock.o: /usr/include/glib-2.0/glib/ghook.h
syscallblock.o: /usr/include/glib-2.0/glib/ghostutils.h
syscallblock.o: /usr/include/glib-2.0/glib/giochannel.h
syscallblock.o: /usr/include/glib-2.0/glib/gmain.h
syscallblock.o: /usr/include/glib-2.0/glib/gpoll.h
syscallblock.o: /usr/include/glib-2.0/glib/gslist.h
syscallblock.o: /usr/include/glib-2.0/glib/gstring.h
syscallblock.o: /usr/include/glib-2.0/glib/gunicode.h
syscallblock.o: /usr/include/glib-2.0/glib/gkeyfile.h
syscallblock.o: /usr/include/glib-2.0/glib/gmappedfile.h
syscallblock.o: /usr/include/glib-2.0/glib/gmarkup.h
syscallblock.o: /usr/include/glib-2.0/glib/gmessages.h
syscallblock.o: /usr/include/glib-2.0/glib/gnode.h
syscallblock.o: /usr/include/glib-2.0/glib/goption.h
syscallblock.o: /usr/include/glib-2.0/glib/gpattern.h
syscallblock.o: /usr/include/glib-2.0/glib/gprimes.h
syscallblock.o: /usr/include/glib-2.0/glib/gqsort.h
syscallblock.o: /usr/include/glib-2.0/glib/gqueue.h
syscallblock.o: /usr/include/glib-2.0/glib/grand.h
syscallblock.o: /usr/include/glib-2.0/glib/grel.h
syscallblock.o: /usr/include/glib-2.0/glib/gregex.h
syscallblock.o: /usr/include/glib-2.0/glib/gscanner.h
syscallblock.o: /usr/include/glib-2.0/glib/gsequence.h
syscallblock.o: /usr/include/glib-2.0/glib/gshell.h
syscallblock.o: /usr/include/glib-2.0/glib/gspawn.h
syscallblock.o: /usr/include/glib-2.0/glib/gstrfuncs.h
syscallblock.o: /usr/include/glib-2.0/glib/gtestutils.h
syscallblock.o: /usr/include/glib-2.0/glib/gthreadpool.h
syscallblock.o: /usr/include/glib-2.0/glib/gtimer.h
syscallblock.o: /usr/include/glib-2.0/glib/gtree.h
syscallblock.o: /usr/include/glib-2.0/glib/gurifuncs.h
syscallblock.o: /usr/include/glib-2.0/glib/gvarianttype.h
syscallblock.o: /usr/include/glib-2.0/glib/gvariant.h
syscallblock.o: /usr/include/glib-2.0/gmodule.h aul/include/aul/common.h
syscallblock.o: /usr/include/string.h aul/include/aul/log.h
syscallblock.o: aul/include/aul/error.h aul/include/aul/string.h
syscallblock.o: aul/include/aul/list.h kernel-priv.h /usr/include/sched.h
syscallblock.o: /usr/include/bits/sched.h ./buffer.h serialize.h
property.o: kernel.h /usr/include/stdlib.h /usr/include/features.h
property.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
property.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
property.o: /usr/include/gnu/stubs-64.h /usr/include/bits/waitflags.h
property.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
property.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
property.o: /usr/include/xlocale.h /usr/include/sys/types.h
property.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
property.o: /usr/include/time.h /usr/include/sys/select.h
property.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
property.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
property.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
property.o: /usr/include/inttypes.h /usr/include/stdint.h
property.o: /usr/include/bits/wchar.h /usr/include/glib-2.0/glib.h
property.o: /usr/include/glib-2.0/glib/galloca.h
property.o: /usr/include/glib-2.0/glib/gtypes.h
property.o: /usr/lib/glib-2.0/include/glibconfig.h
property.o: /usr/include/glib-2.0/glib/gmacros.h /usr/include/limits.h
property.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
property.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
property.o: /usr/include/bits/xopen_lim.h /usr/include/bits/stdio_lim.h
property.o: /usr/include/glib-2.0/glib/garray.h
property.o: /usr/include/glib-2.0/glib/gasyncqueue.h
property.o: /usr/include/glib-2.0/glib/gthread.h
property.o: /usr/include/glib-2.0/glib/gerror.h
property.o: /usr/include/glib-2.0/glib/gquark.h
property.o: /usr/include/glib-2.0/glib/gutils.h
property.o: /usr/include/glib-2.0/glib/gatomic.h
property.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
property.o: /usr/include/glib-2.0/glib/gbase64.h
property.o: /usr/include/glib-2.0/glib/gbitlock.h
property.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
property.o: /usr/include/glib-2.0/glib/gcache.h
property.o: /usr/include/glib-2.0/glib/glist.h
property.o: /usr/include/glib-2.0/glib/gmem.h
property.o: /usr/include/glib-2.0/glib/gslice.h
property.o: /usr/include/glib-2.0/glib/gchecksum.h
property.o: /usr/include/glib-2.0/glib/gcompletion.h
property.o: /usr/include/glib-2.0/glib/gconvert.h
property.o: /usr/include/glib-2.0/glib/gdataset.h
property.o: /usr/include/glib-2.0/glib/gdate.h
property.o: /usr/include/glib-2.0/glib/gdir.h
property.o: /usr/include/glib-2.0/glib/gfileutils.h
property.o: /usr/include/glib-2.0/glib/ghash.h
property.o: /usr/include/glib-2.0/glib/ghook.h
property.o: /usr/include/glib-2.0/glib/ghostutils.h
property.o: /usr/include/glib-2.0/glib/giochannel.h
property.o: /usr/include/glib-2.0/glib/gmain.h
property.o: /usr/include/glib-2.0/glib/gpoll.h
property.o: /usr/include/glib-2.0/glib/gslist.h
property.o: /usr/include/glib-2.0/glib/gstring.h
property.o: /usr/include/glib-2.0/glib/gunicode.h
property.o: /usr/include/glib-2.0/glib/gkeyfile.h
property.o: /usr/include/glib-2.0/glib/gmappedfile.h
property.o: /usr/include/glib-2.0/glib/gmarkup.h
property.o: /usr/include/glib-2.0/glib/gmessages.h
property.o: /usr/include/glib-2.0/glib/gnode.h
property.o: /usr/include/glib-2.0/glib/goption.h
property.o: /usr/include/glib-2.0/glib/gpattern.h
property.o: /usr/include/glib-2.0/glib/gprimes.h
property.o: /usr/include/glib-2.0/glib/gqsort.h
property.o: /usr/include/glib-2.0/glib/gqueue.h
property.o: /usr/include/glib-2.0/glib/grand.h
property.o: /usr/include/glib-2.0/glib/grel.h
property.o: /usr/include/glib-2.0/glib/gregex.h
property.o: /usr/include/glib-2.0/glib/gscanner.h
property.o: /usr/include/glib-2.0/glib/gsequence.h
property.o: /usr/include/glib-2.0/glib/gshell.h
property.o: /usr/include/glib-2.0/glib/gspawn.h
property.o: /usr/include/glib-2.0/glib/gstrfuncs.h
property.o: /usr/include/glib-2.0/glib/gtestutils.h
property.o: /usr/include/glib-2.0/glib/gthreadpool.h
property.o: /usr/include/glib-2.0/glib/gtimer.h
property.o: /usr/include/glib-2.0/glib/gtree.h
property.o: /usr/include/glib-2.0/glib/gurifuncs.h
property.o: /usr/include/glib-2.0/glib/gvarianttype.h
property.o: /usr/include/glib-2.0/glib/gvariant.h
property.o: /usr/include/glib-2.0/gmodule.h aul/include/aul/common.h
property.o: /usr/include/string.h aul/include/aul/log.h
property.o: aul/include/aul/error.h aul/include/aul/string.h
property.o: aul/include/aul/list.h
config.o: /usr/include/stdlib.h /usr/include/features.h
config.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
config.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
config.o: /usr/include/gnu/stubs-64.h /usr/include/bits/waitflags.h
config.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
config.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
config.o: /usr/include/xlocale.h /usr/include/sys/types.h
config.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
config.o: /usr/include/time.h /usr/include/sys/select.h
config.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
config.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
config.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
config.o: /usr/include/string.h kernel.h /usr/include/inttypes.h
config.o: /usr/include/stdint.h /usr/include/bits/wchar.h
config.o: /usr/include/glib-2.0/glib.h /usr/include/glib-2.0/glib/galloca.h
config.o: /usr/include/glib-2.0/glib/gtypes.h
config.o: /usr/lib/glib-2.0/include/glibconfig.h
config.o: /usr/include/glib-2.0/glib/gmacros.h /usr/include/limits.h
config.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
config.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
config.o: /usr/include/bits/xopen_lim.h /usr/include/bits/stdio_lim.h
config.o: /usr/include/glib-2.0/glib/garray.h
config.o: /usr/include/glib-2.0/glib/gasyncqueue.h
config.o: /usr/include/glib-2.0/glib/gthread.h
config.o: /usr/include/glib-2.0/glib/gerror.h
config.o: /usr/include/glib-2.0/glib/gquark.h
config.o: /usr/include/glib-2.0/glib/gutils.h
config.o: /usr/include/glib-2.0/glib/gatomic.h
config.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
config.o: /usr/include/glib-2.0/glib/gbase64.h
config.o: /usr/include/glib-2.0/glib/gbitlock.h
config.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
config.o: /usr/include/glib-2.0/glib/gcache.h
config.o: /usr/include/glib-2.0/glib/glist.h
config.o: /usr/include/glib-2.0/glib/gmem.h
config.o: /usr/include/glib-2.0/glib/gslice.h
config.o: /usr/include/glib-2.0/glib/gchecksum.h
config.o: /usr/include/glib-2.0/glib/gcompletion.h
config.o: /usr/include/glib-2.0/glib/gconvert.h
config.o: /usr/include/glib-2.0/glib/gdataset.h
config.o: /usr/include/glib-2.0/glib/gdate.h
config.o: /usr/include/glib-2.0/glib/gdir.h
config.o: /usr/include/glib-2.0/glib/gfileutils.h
config.o: /usr/include/glib-2.0/glib/ghash.h
config.o: /usr/include/glib-2.0/glib/ghook.h
config.o: /usr/include/glib-2.0/glib/ghostutils.h
config.o: /usr/include/glib-2.0/glib/giochannel.h
config.o: /usr/include/glib-2.0/glib/gmain.h
config.o: /usr/include/glib-2.0/glib/gpoll.h
config.o: /usr/include/glib-2.0/glib/gslist.h
config.o: /usr/include/glib-2.0/glib/gstring.h
config.o: /usr/include/glib-2.0/glib/gunicode.h
config.o: /usr/include/glib-2.0/glib/gkeyfile.h
config.o: /usr/include/glib-2.0/glib/gmappedfile.h
config.o: /usr/include/glib-2.0/glib/gmarkup.h
config.o: /usr/include/glib-2.0/glib/gmessages.h
config.o: /usr/include/glib-2.0/glib/gnode.h
config.o: /usr/include/glib-2.0/glib/goption.h
config.o: /usr/include/glib-2.0/glib/gpattern.h
config.o: /usr/include/glib-2.0/glib/gprimes.h
config.o: /usr/include/glib-2.0/glib/gqsort.h
config.o: /usr/include/glib-2.0/glib/gqueue.h
config.o: /usr/include/glib-2.0/glib/grand.h
config.o: /usr/include/glib-2.0/glib/grel.h
config.o: /usr/include/glib-2.0/glib/gregex.h
config.o: /usr/include/glib-2.0/glib/gscanner.h
config.o: /usr/include/glib-2.0/glib/gsequence.h
config.o: /usr/include/glib-2.0/glib/gshell.h
config.o: /usr/include/glib-2.0/glib/gspawn.h
config.o: /usr/include/glib-2.0/glib/gstrfuncs.h
config.o: /usr/include/glib-2.0/glib/gtestutils.h
config.o: /usr/include/glib-2.0/glib/gthreadpool.h
config.o: /usr/include/glib-2.0/glib/gtimer.h
config.o: /usr/include/glib-2.0/glib/gtree.h
config.o: /usr/include/glib-2.0/glib/gurifuncs.h
config.o: /usr/include/glib-2.0/glib/gvarianttype.h
config.o: /usr/include/glib-2.0/glib/gvariant.h
config.o: /usr/include/glib-2.0/gmodule.h aul/include/aul/common.h
config.o: aul/include/aul/log.h aul/include/aul/error.h
config.o: aul/include/aul/string.h aul/include/aul/list.h kernel-priv.h
config.o: /usr/include/sched.h /usr/include/bits/sched.h
calibration.o: /usr/include/stdio.h /usr/include/features.h
calibration.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
calibration.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
calibration.o: /usr/include/gnu/stubs-64.h /usr/include/bits/types.h
calibration.o: /usr/include/bits/typesizes.h /usr/include/libio.h
calibration.o: /usr/include/_G_config.h /usr/include/wchar.h
calibration.o: /usr/include/bits/stdio_lim.h /usr/include/bits/sys_errlist.h
calibration.o: /usr/include/sqlite3.h /usr/include/string.h
calibration.o: /usr/include/xlocale.h /usr/include/limits.h
calibration.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
calibration.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
calibration.o: /usr/include/bits/xopen_lim.h aul/include/aul/common.h
calibration.o: /usr/include/stdlib.h /usr/include/bits/waitflags.h
calibration.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
calibration.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
calibration.o: /usr/include/sys/types.h /usr/include/time.h
calibration.o: /usr/include/sys/select.h /usr/include/bits/select.h
calibration.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
calibration.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
calibration.o: /usr/include/alloca.h /usr/include/inttypes.h
calibration.o: /usr/include/stdint.h /usr/include/bits/wchar.h kernel.h
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
calibration.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
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
calibration.o: /usr/include/glib-2.0/glib/gvariant.h
calibration.o: /usr/include/glib-2.0/gmodule.h aul/include/aul/log.h
calibration.o: aul/include/aul/error.h aul/include/aul/string.h
calibration.o: aul/include/aul/list.h kernel-priv.h /usr/include/sched.h
calibration.o: /usr/include/bits/sched.h
buffer.o: /usr/include/string.h /usr/include/features.h
buffer.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
buffer.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
buffer.o: /usr/include/gnu/stubs-64.h /usr/include/xlocale.h
buffer.o: /usr/include/glib-2.0/glib.h /usr/include/glib-2.0/glib/galloca.h
buffer.o: /usr/include/glib-2.0/glib/gtypes.h
buffer.o: /usr/lib/glib-2.0/include/glibconfig.h
buffer.o: /usr/include/glib-2.0/glib/gmacros.h /usr/include/limits.h
buffer.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
buffer.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
buffer.o: /usr/include/bits/xopen_lim.h /usr/include/bits/stdio_lim.h
buffer.o: /usr/include/glib-2.0/glib/garray.h
buffer.o: /usr/include/glib-2.0/glib/gasyncqueue.h
buffer.o: /usr/include/glib-2.0/glib/gthread.h
buffer.o: /usr/include/glib-2.0/glib/gerror.h
buffer.o: /usr/include/glib-2.0/glib/gquark.h
buffer.o: /usr/include/glib-2.0/glib/gutils.h
buffer.o: /usr/include/glib-2.0/glib/gatomic.h
buffer.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
buffer.o: /usr/include/bits/sigset.h /usr/include/glib-2.0/glib/gbase64.h
buffer.o: /usr/include/glib-2.0/glib/gbitlock.h
buffer.o: /usr/include/glib-2.0/glib/gbookmarkfile.h /usr/include/time.h
buffer.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
buffer.o: /usr/include/glib-2.0/glib/gcache.h
buffer.o: /usr/include/glib-2.0/glib/glist.h
buffer.o: /usr/include/glib-2.0/glib/gmem.h
buffer.o: /usr/include/glib-2.0/glib/gslice.h
buffer.o: /usr/include/glib-2.0/glib/gchecksum.h
buffer.o: /usr/include/glib-2.0/glib/gcompletion.h
buffer.o: /usr/include/glib-2.0/glib/gconvert.h
buffer.o: /usr/include/glib-2.0/glib/gdataset.h
buffer.o: /usr/include/glib-2.0/glib/gdate.h
buffer.o: /usr/include/glib-2.0/glib/gdir.h
buffer.o: /usr/include/glib-2.0/glib/gfileutils.h
buffer.o: /usr/include/glib-2.0/glib/ghash.h
buffer.o: /usr/include/glib-2.0/glib/ghook.h
buffer.o: /usr/include/glib-2.0/glib/ghostutils.h
buffer.o: /usr/include/glib-2.0/glib/giochannel.h
buffer.o: /usr/include/glib-2.0/glib/gmain.h
buffer.o: /usr/include/glib-2.0/glib/gpoll.h
buffer.o: /usr/include/glib-2.0/glib/gslist.h
buffer.o: /usr/include/glib-2.0/glib/gstring.h
buffer.o: /usr/include/glib-2.0/glib/gunicode.h
buffer.o: /usr/include/glib-2.0/glib/gkeyfile.h
buffer.o: /usr/include/glib-2.0/glib/gmappedfile.h
buffer.o: /usr/include/glib-2.0/glib/gmarkup.h
buffer.o: /usr/include/glib-2.0/glib/gmessages.h
buffer.o: /usr/include/glib-2.0/glib/gnode.h
buffer.o: /usr/include/glib-2.0/glib/goption.h
buffer.o: /usr/include/glib-2.0/glib/gpattern.h
buffer.o: /usr/include/glib-2.0/glib/gprimes.h
buffer.o: /usr/include/glib-2.0/glib/gqsort.h
buffer.o: /usr/include/glib-2.0/glib/gqueue.h
buffer.o: /usr/include/glib-2.0/glib/grand.h
buffer.o: /usr/include/glib-2.0/glib/grel.h
buffer.o: /usr/include/glib-2.0/glib/gregex.h
buffer.o: /usr/include/glib-2.0/glib/gscanner.h
buffer.o: /usr/include/glib-2.0/glib/gsequence.h
buffer.o: /usr/include/glib-2.0/glib/gshell.h
buffer.o: /usr/include/glib-2.0/glib/gspawn.h
buffer.o: /usr/include/glib-2.0/glib/gstrfuncs.h
buffer.o: /usr/include/glib-2.0/glib/gtestutils.h
buffer.o: /usr/include/glib-2.0/glib/gthreadpool.h
buffer.o: /usr/include/glib-2.0/glib/gtimer.h
buffer.o: /usr/include/glib-2.0/glib/gtree.h
buffer.o: /usr/include/glib-2.0/glib/gurifuncs.h
buffer.o: /usr/include/glib-2.0/glib/gvarianttype.h
buffer.o: /usr/include/glib-2.0/glib/gvariant.h ./buffer.h kernel.h
buffer.o: /usr/include/stdlib.h /usr/include/bits/waitflags.h
buffer.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
buffer.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
buffer.o: /usr/include/sys/types.h /usr/include/sys/select.h
buffer.o: /usr/include/bits/select.h /usr/include/bits/time.h
buffer.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
buffer.o: /usr/include/alloca.h /usr/include/inttypes.h /usr/include/stdint.h
buffer.o: /usr/include/bits/wchar.h /usr/include/glib-2.0/gmodule.h
buffer.o: aul/include/aul/common.h aul/include/aul/log.h
buffer.o: aul/include/aul/error.h aul/include/aul/string.h
buffer.o: aul/include/aul/list.h kernel-priv.h /usr/include/sched.h
buffer.o: /usr/include/bits/sched.h
serialize.o: /usr/include/stdlib.h /usr/include/features.h
serialize.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
serialize.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
serialize.o: /usr/include/gnu/stubs-64.h /usr/include/bits/waitflags.h
serialize.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
serialize.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
serialize.o: /usr/include/xlocale.h /usr/include/sys/types.h
serialize.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
serialize.o: /usr/include/time.h /usr/include/sys/select.h
serialize.o: /usr/include/bits/select.h /usr/include/bits/sigset.h
serialize.o: /usr/include/bits/time.h /usr/include/sys/sysmacros.h
serialize.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
serialize.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
serialize.o: /usr/include/bits/environments.h /usr/include/bits/confname.h
serialize.o: /usr/include/getopt.h /usr/include/string.h
serialize.o: /usr/include/glib-2.0/glib.h
serialize.o: /usr/include/glib-2.0/glib/galloca.h
serialize.o: /usr/include/glib-2.0/glib/gtypes.h
serialize.o: /usr/lib/glib-2.0/include/glibconfig.h
serialize.o: /usr/include/glib-2.0/glib/gmacros.h /usr/include/limits.h
serialize.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
serialize.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
serialize.o: /usr/include/bits/xopen_lim.h /usr/include/bits/stdio_lim.h
serialize.o: /usr/include/glib-2.0/glib/garray.h
serialize.o: /usr/include/glib-2.0/glib/gasyncqueue.h
serialize.o: /usr/include/glib-2.0/glib/gthread.h
serialize.o: /usr/include/glib-2.0/glib/gerror.h
serialize.o: /usr/include/glib-2.0/glib/gquark.h
serialize.o: /usr/include/glib-2.0/glib/gutils.h
serialize.o: /usr/include/glib-2.0/glib/gatomic.h
serialize.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
serialize.o: /usr/include/glib-2.0/glib/gbase64.h
serialize.o: /usr/include/glib-2.0/glib/gbitlock.h
serialize.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
serialize.o: /usr/include/glib-2.0/glib/gcache.h
serialize.o: /usr/include/glib-2.0/glib/glist.h
serialize.o: /usr/include/glib-2.0/glib/gmem.h
serialize.o: /usr/include/glib-2.0/glib/gslice.h
serialize.o: /usr/include/glib-2.0/glib/gchecksum.h
serialize.o: /usr/include/glib-2.0/glib/gcompletion.h
serialize.o: /usr/include/glib-2.0/glib/gconvert.h
serialize.o: /usr/include/glib-2.0/glib/gdataset.h
serialize.o: /usr/include/glib-2.0/glib/gdate.h
serialize.o: /usr/include/glib-2.0/glib/gdir.h
serialize.o: /usr/include/glib-2.0/glib/gfileutils.h
serialize.o: /usr/include/glib-2.0/glib/ghash.h
serialize.o: /usr/include/glib-2.0/glib/ghook.h
serialize.o: /usr/include/glib-2.0/glib/ghostutils.h
serialize.o: /usr/include/glib-2.0/glib/giochannel.h
serialize.o: /usr/include/glib-2.0/glib/gmain.h
serialize.o: /usr/include/glib-2.0/glib/gpoll.h
serialize.o: /usr/include/glib-2.0/glib/gslist.h
serialize.o: /usr/include/glib-2.0/glib/gstring.h
serialize.o: /usr/include/glib-2.0/glib/gunicode.h
serialize.o: /usr/include/glib-2.0/glib/gkeyfile.h
serialize.o: /usr/include/glib-2.0/glib/gmappedfile.h
serialize.o: /usr/include/glib-2.0/glib/gmarkup.h
serialize.o: /usr/include/glib-2.0/glib/gmessages.h
serialize.o: /usr/include/glib-2.0/glib/gnode.h
serialize.o: /usr/include/glib-2.0/glib/goption.h
serialize.o: /usr/include/glib-2.0/glib/gpattern.h
serialize.o: /usr/include/glib-2.0/glib/gprimes.h
serialize.o: /usr/include/glib-2.0/glib/gqsort.h
serialize.o: /usr/include/glib-2.0/glib/gqueue.h
serialize.o: /usr/include/glib-2.0/glib/grand.h
serialize.o: /usr/include/glib-2.0/glib/grel.h
serialize.o: /usr/include/glib-2.0/glib/gregex.h
serialize.o: /usr/include/glib-2.0/glib/gscanner.h
serialize.o: /usr/include/glib-2.0/glib/gsequence.h
serialize.o: /usr/include/glib-2.0/glib/gshell.h
serialize.o: /usr/include/glib-2.0/glib/gspawn.h
serialize.o: /usr/include/glib-2.0/glib/gstrfuncs.h
serialize.o: /usr/include/glib-2.0/glib/gtestutils.h
serialize.o: /usr/include/glib-2.0/glib/gthreadpool.h
serialize.o: /usr/include/glib-2.0/glib/gtimer.h
serialize.o: /usr/include/glib-2.0/glib/gtree.h
serialize.o: /usr/include/glib-2.0/glib/gurifuncs.h
serialize.o: /usr/include/glib-2.0/glib/gvarianttype.h
serialize.o: /usr/include/glib-2.0/glib/gvariant.h array.h ./buffer.h
serialize.o: serialize.h kernel.h /usr/include/inttypes.h
serialize.o: /usr/include/stdint.h /usr/include/bits/wchar.h
serialize.o: /usr/include/glib-2.0/gmodule.h aul/include/aul/common.h
serialize.o: aul/include/aul/log.h aul/include/aul/error.h
serialize.o: aul/include/aul/string.h aul/include/aul/list.h
trigger.o: /usr/include/unistd.h /usr/include/features.h
trigger.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
trigger.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
trigger.o: /usr/include/gnu/stubs-64.h /usr/include/bits/posix_opt.h
trigger.o: /usr/include/bits/environments.h /usr/include/bits/types.h
trigger.o: /usr/include/bits/typesizes.h /usr/include/bits/confname.h
trigger.o: /usr/include/getopt.h /usr/include/string.h /usr/include/xlocale.h
trigger.o: kernel.h /usr/include/stdlib.h /usr/include/bits/waitflags.h
trigger.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
trigger.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
trigger.o: /usr/include/sys/types.h /usr/include/time.h
trigger.o: /usr/include/sys/select.h /usr/include/bits/select.h
trigger.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
trigger.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
trigger.o: /usr/include/alloca.h /usr/include/inttypes.h
trigger.o: /usr/include/stdint.h /usr/include/bits/wchar.h
trigger.o: /usr/include/glib-2.0/glib.h /usr/include/glib-2.0/glib/galloca.h
trigger.o: /usr/include/glib-2.0/glib/gtypes.h
trigger.o: /usr/lib/glib-2.0/include/glibconfig.h
trigger.o: /usr/include/glib-2.0/glib/gmacros.h /usr/include/limits.h
trigger.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
trigger.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
trigger.o: /usr/include/bits/xopen_lim.h /usr/include/bits/stdio_lim.h
trigger.o: /usr/include/glib-2.0/glib/garray.h
trigger.o: /usr/include/glib-2.0/glib/gasyncqueue.h
trigger.o: /usr/include/glib-2.0/glib/gthread.h
trigger.o: /usr/include/glib-2.0/glib/gerror.h
trigger.o: /usr/include/glib-2.0/glib/gquark.h
trigger.o: /usr/include/glib-2.0/glib/gutils.h
trigger.o: /usr/include/glib-2.0/glib/gatomic.h
trigger.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
trigger.o: /usr/include/glib-2.0/glib/gbase64.h
trigger.o: /usr/include/glib-2.0/glib/gbitlock.h
trigger.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
trigger.o: /usr/include/glib-2.0/glib/gcache.h
trigger.o: /usr/include/glib-2.0/glib/glist.h
trigger.o: /usr/include/glib-2.0/glib/gmem.h
trigger.o: /usr/include/glib-2.0/glib/gslice.h
trigger.o: /usr/include/glib-2.0/glib/gchecksum.h
trigger.o: /usr/include/glib-2.0/glib/gcompletion.h
trigger.o: /usr/include/glib-2.0/glib/gconvert.h
trigger.o: /usr/include/glib-2.0/glib/gdataset.h
trigger.o: /usr/include/glib-2.0/glib/gdate.h
trigger.o: /usr/include/glib-2.0/glib/gdir.h
trigger.o: /usr/include/glib-2.0/glib/gfileutils.h
trigger.o: /usr/include/glib-2.0/glib/ghash.h
trigger.o: /usr/include/glib-2.0/glib/ghook.h
trigger.o: /usr/include/glib-2.0/glib/ghostutils.h
trigger.o: /usr/include/glib-2.0/glib/giochannel.h
trigger.o: /usr/include/glib-2.0/glib/gmain.h
trigger.o: /usr/include/glib-2.0/glib/gpoll.h
trigger.o: /usr/include/glib-2.0/glib/gslist.h
trigger.o: /usr/include/glib-2.0/glib/gstring.h
trigger.o: /usr/include/glib-2.0/glib/gunicode.h
trigger.o: /usr/include/glib-2.0/glib/gkeyfile.h
trigger.o: /usr/include/glib-2.0/glib/gmappedfile.h
trigger.o: /usr/include/glib-2.0/glib/gmarkup.h
trigger.o: /usr/include/glib-2.0/glib/gmessages.h
trigger.o: /usr/include/glib-2.0/glib/gnode.h
trigger.o: /usr/include/glib-2.0/glib/goption.h
trigger.o: /usr/include/glib-2.0/glib/gpattern.h
trigger.o: /usr/include/glib-2.0/glib/gprimes.h
trigger.o: /usr/include/glib-2.0/glib/gqsort.h
trigger.o: /usr/include/glib-2.0/glib/gqueue.h
trigger.o: /usr/include/glib-2.0/glib/grand.h
trigger.o: /usr/include/glib-2.0/glib/grel.h
trigger.o: /usr/include/glib-2.0/glib/gregex.h
trigger.o: /usr/include/glib-2.0/glib/gscanner.h
trigger.o: /usr/include/glib-2.0/glib/gsequence.h
trigger.o: /usr/include/glib-2.0/glib/gshell.h
trigger.o: /usr/include/glib-2.0/glib/gspawn.h
trigger.o: /usr/include/glib-2.0/glib/gstrfuncs.h
trigger.o: /usr/include/glib-2.0/glib/gtestutils.h
trigger.o: /usr/include/glib-2.0/glib/gthreadpool.h
trigger.o: /usr/include/glib-2.0/glib/gtimer.h
trigger.o: /usr/include/glib-2.0/glib/gtree.h
trigger.o: /usr/include/glib-2.0/glib/gurifuncs.h
trigger.o: /usr/include/glib-2.0/glib/gvarianttype.h
trigger.o: /usr/include/glib-2.0/glib/gvariant.h
trigger.o: /usr/include/glib-2.0/gmodule.h aul/include/aul/common.h
trigger.o: aul/include/aul/log.h aul/include/aul/error.h
trigger.o: aul/include/aul/string.h aul/include/aul/list.h kernel-priv.h
trigger.o: /usr/include/sched.h /usr/include/bits/sched.h
exec.o: /usr/include/string.h /usr/include/features.h
exec.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
exec.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
exec.o: /usr/include/gnu/stubs-64.h /usr/include/xlocale.h kernel.h
exec.o: /usr/include/stdlib.h /usr/include/bits/waitflags.h
exec.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
exec.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
exec.o: /usr/include/sys/types.h /usr/include/bits/types.h
exec.o: /usr/include/bits/typesizes.h /usr/include/time.h
exec.o: /usr/include/sys/select.h /usr/include/bits/select.h
exec.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
exec.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
exec.o: /usr/include/alloca.h /usr/include/inttypes.h /usr/include/stdint.h
exec.o: /usr/include/bits/wchar.h /usr/include/glib-2.0/glib.h
exec.o: /usr/include/glib-2.0/glib/galloca.h
exec.o: /usr/include/glib-2.0/glib/gtypes.h
exec.o: /usr/lib/glib-2.0/include/glibconfig.h
exec.o: /usr/include/glib-2.0/glib/gmacros.h /usr/include/limits.h
exec.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
exec.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
exec.o: /usr/include/bits/xopen_lim.h /usr/include/bits/stdio_lim.h
exec.o: /usr/include/glib-2.0/glib/garray.h
exec.o: /usr/include/glib-2.0/glib/gasyncqueue.h
exec.o: /usr/include/glib-2.0/glib/gthread.h
exec.o: /usr/include/glib-2.0/glib/gerror.h
exec.o: /usr/include/glib-2.0/glib/gquark.h
exec.o: /usr/include/glib-2.0/glib/gutils.h
exec.o: /usr/include/glib-2.0/glib/gatomic.h
exec.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
exec.o: /usr/include/glib-2.0/glib/gbase64.h
exec.o: /usr/include/glib-2.0/glib/gbitlock.h
exec.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
exec.o: /usr/include/glib-2.0/glib/gcache.h
exec.o: /usr/include/glib-2.0/glib/glist.h /usr/include/glib-2.0/glib/gmem.h
exec.o: /usr/include/glib-2.0/glib/gslice.h
exec.o: /usr/include/glib-2.0/glib/gchecksum.h
exec.o: /usr/include/glib-2.0/glib/gcompletion.h
exec.o: /usr/include/glib-2.0/glib/gconvert.h
exec.o: /usr/include/glib-2.0/glib/gdataset.h
exec.o: /usr/include/glib-2.0/glib/gdate.h /usr/include/glib-2.0/glib/gdir.h
exec.o: /usr/include/glib-2.0/glib/gfileutils.h
exec.o: /usr/include/glib-2.0/glib/ghash.h /usr/include/glib-2.0/glib/ghook.h
exec.o: /usr/include/glib-2.0/glib/ghostutils.h
exec.o: /usr/include/glib-2.0/glib/giochannel.h
exec.o: /usr/include/glib-2.0/glib/gmain.h /usr/include/glib-2.0/glib/gpoll.h
exec.o: /usr/include/glib-2.0/glib/gslist.h
exec.o: /usr/include/glib-2.0/glib/gstring.h
exec.o: /usr/include/glib-2.0/glib/gunicode.h
exec.o: /usr/include/glib-2.0/glib/gkeyfile.h
exec.o: /usr/include/glib-2.0/glib/gmappedfile.h
exec.o: /usr/include/glib-2.0/glib/gmarkup.h
exec.o: /usr/include/glib-2.0/glib/gmessages.h
exec.o: /usr/include/glib-2.0/glib/gnode.h
exec.o: /usr/include/glib-2.0/glib/goption.h
exec.o: /usr/include/glib-2.0/glib/gpattern.h
exec.o: /usr/include/glib-2.0/glib/gprimes.h
exec.o: /usr/include/glib-2.0/glib/gqsort.h
exec.o: /usr/include/glib-2.0/glib/gqueue.h
exec.o: /usr/include/glib-2.0/glib/grand.h /usr/include/glib-2.0/glib/grel.h
exec.o: /usr/include/glib-2.0/glib/gregex.h
exec.o: /usr/include/glib-2.0/glib/gscanner.h
exec.o: /usr/include/glib-2.0/glib/gsequence.h
exec.o: /usr/include/glib-2.0/glib/gshell.h
exec.o: /usr/include/glib-2.0/glib/gspawn.h
exec.o: /usr/include/glib-2.0/glib/gstrfuncs.h
exec.o: /usr/include/glib-2.0/glib/gtestutils.h
exec.o: /usr/include/glib-2.0/glib/gthreadpool.h
exec.o: /usr/include/glib-2.0/glib/gtimer.h
exec.o: /usr/include/glib-2.0/glib/gtree.h
exec.o: /usr/include/glib-2.0/glib/gurifuncs.h
exec.o: /usr/include/glib-2.0/glib/gvarianttype.h
exec.o: /usr/include/glib-2.0/glib/gvariant.h /usr/include/glib-2.0/gmodule.h
exec.o: aul/include/aul/common.h aul/include/aul/log.h
exec.o: aul/include/aul/error.h aul/include/aul/string.h
exec.o: aul/include/aul/list.h kernel-priv.h /usr/include/sched.h
exec.o: /usr/include/bits/sched.h
luaenv.o: /usr/include/string.h /usr/include/features.h
luaenv.o: /usr/include/bits/predefs.h /usr/include/sys/cdefs.h
luaenv.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
luaenv.o: /usr/include/gnu/stubs-64.h /usr/include/xlocale.h
luaenv.o: /usr/include/lua5.1/lua.h /usr/include/lua5.1/luaconf.h
luaenv.o: /usr/include/limits.h /usr/include/bits/posix1_lim.h
luaenv.o: /usr/include/bits/local_lim.h /usr/include/linux/limits.h
luaenv.o: /usr/include/bits/posix2_lim.h /usr/include/bits/xopen_lim.h
luaenv.o: /usr/include/bits/stdio_lim.h /usr/include/lua5.1/lauxlib.h
luaenv.o: /usr/include/stdio.h /usr/include/bits/types.h
luaenv.o: /usr/include/bits/typesizes.h /usr/include/libio.h
luaenv.o: /usr/include/_G_config.h /usr/include/wchar.h
luaenv.o: /usr/include/bits/sys_errlist.h /usr/include/lua5.1/lualib.h
luaenv.o: kernel.h /usr/include/stdlib.h /usr/include/bits/waitflags.h
luaenv.o: /usr/include/bits/waitstatus.h /usr/include/endian.h
luaenv.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
luaenv.o: /usr/include/sys/types.h /usr/include/time.h
luaenv.o: /usr/include/sys/select.h /usr/include/bits/select.h
luaenv.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
luaenv.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
luaenv.o: /usr/include/alloca.h /usr/include/inttypes.h /usr/include/stdint.h
luaenv.o: /usr/include/bits/wchar.h /usr/include/glib-2.0/glib.h
luaenv.o: /usr/include/glib-2.0/glib/galloca.h
luaenv.o: /usr/include/glib-2.0/glib/gtypes.h
luaenv.o: /usr/lib/glib-2.0/include/glibconfig.h
luaenv.o: /usr/include/glib-2.0/glib/gmacros.h
luaenv.o: /usr/include/glib-2.0/glib/garray.h
luaenv.o: /usr/include/glib-2.0/glib/gasyncqueue.h
luaenv.o: /usr/include/glib-2.0/glib/gthread.h
luaenv.o: /usr/include/glib-2.0/glib/gerror.h
luaenv.o: /usr/include/glib-2.0/glib/gquark.h
luaenv.o: /usr/include/glib-2.0/glib/gutils.h
luaenv.o: /usr/include/glib-2.0/glib/gatomic.h
luaenv.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
luaenv.o: /usr/include/glib-2.0/glib/gbase64.h
luaenv.o: /usr/include/glib-2.0/glib/gbitlock.h
luaenv.o: /usr/include/glib-2.0/glib/gbookmarkfile.h
luaenv.o: /usr/include/glib-2.0/glib/gcache.h
luaenv.o: /usr/include/glib-2.0/glib/glist.h
luaenv.o: /usr/include/glib-2.0/glib/gmem.h
luaenv.o: /usr/include/glib-2.0/glib/gslice.h
luaenv.o: /usr/include/glib-2.0/glib/gchecksum.h
luaenv.o: /usr/include/glib-2.0/glib/gcompletion.h
luaenv.o: /usr/include/glib-2.0/glib/gconvert.h
luaenv.o: /usr/include/glib-2.0/glib/gdataset.h
luaenv.o: /usr/include/glib-2.0/glib/gdate.h
luaenv.o: /usr/include/glib-2.0/glib/gdir.h
luaenv.o: /usr/include/glib-2.0/glib/gfileutils.h
luaenv.o: /usr/include/glib-2.0/glib/ghash.h
luaenv.o: /usr/include/glib-2.0/glib/ghook.h
luaenv.o: /usr/include/glib-2.0/glib/ghostutils.h
luaenv.o: /usr/include/glib-2.0/glib/giochannel.h
luaenv.o: /usr/include/glib-2.0/glib/gmain.h
luaenv.o: /usr/include/glib-2.0/glib/gpoll.h
luaenv.o: /usr/include/glib-2.0/glib/gslist.h
luaenv.o: /usr/include/glib-2.0/glib/gstring.h
luaenv.o: /usr/include/glib-2.0/glib/gunicode.h
luaenv.o: /usr/include/glib-2.0/glib/gkeyfile.h
luaenv.o: /usr/include/glib-2.0/glib/gmappedfile.h
luaenv.o: /usr/include/glib-2.0/glib/gmarkup.h
luaenv.o: /usr/include/glib-2.0/glib/gmessages.h
luaenv.o: /usr/include/glib-2.0/glib/gnode.h
luaenv.o: /usr/include/glib-2.0/glib/goption.h
luaenv.o: /usr/include/glib-2.0/glib/gpattern.h
luaenv.o: /usr/include/glib-2.0/glib/gprimes.h
luaenv.o: /usr/include/glib-2.0/glib/gqsort.h
luaenv.o: /usr/include/glib-2.0/glib/gqueue.h
luaenv.o: /usr/include/glib-2.0/glib/grand.h
luaenv.o: /usr/include/glib-2.0/glib/grel.h
luaenv.o: /usr/include/glib-2.0/glib/gregex.h
luaenv.o: /usr/include/glib-2.0/glib/gscanner.h
luaenv.o: /usr/include/glib-2.0/glib/gsequence.h
luaenv.o: /usr/include/glib-2.0/glib/gshell.h
luaenv.o: /usr/include/glib-2.0/glib/gspawn.h
luaenv.o: /usr/include/glib-2.0/glib/gstrfuncs.h
luaenv.o: /usr/include/glib-2.0/glib/gtestutils.h
luaenv.o: /usr/include/glib-2.0/glib/gthreadpool.h
luaenv.o: /usr/include/glib-2.0/glib/gtimer.h
luaenv.o: /usr/include/glib-2.0/glib/gtree.h
luaenv.o: /usr/include/glib-2.0/glib/gurifuncs.h
luaenv.o: /usr/include/glib-2.0/glib/gvarianttype.h
luaenv.o: /usr/include/glib-2.0/glib/gvariant.h
luaenv.o: /usr/include/glib-2.0/gmodule.h aul/include/aul/common.h
luaenv.o: aul/include/aul/log.h aul/include/aul/error.h
luaenv.o: aul/include/aul/string.h aul/include/aul/list.h kernel-priv.h
luaenv.o: /usr/include/sched.h /usr/include/bits/sched.h
math.o: /usr/include/matheval.h /usr/include/glib-2.0/glib.h
math.o: /usr/include/glib-2.0/glib/galloca.h
math.o: /usr/include/glib-2.0/glib/gtypes.h
math.o: /usr/lib/glib-2.0/include/glibconfig.h
math.o: /usr/include/glib-2.0/glib/gmacros.h /usr/include/limits.h
math.o: /usr/include/features.h /usr/include/bits/predefs.h
math.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
math.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-64.h
math.o: /usr/include/bits/posix1_lim.h /usr/include/bits/local_lim.h
math.o: /usr/include/linux/limits.h /usr/include/bits/posix2_lim.h
math.o: /usr/include/bits/xopen_lim.h /usr/include/bits/stdio_lim.h
math.o: /usr/include/glib-2.0/glib/garray.h
math.o: /usr/include/glib-2.0/glib/gasyncqueue.h
math.o: /usr/include/glib-2.0/glib/gthread.h
math.o: /usr/include/glib-2.0/glib/gerror.h
math.o: /usr/include/glib-2.0/glib/gquark.h
math.o: /usr/include/glib-2.0/glib/gutils.h
math.o: /usr/include/glib-2.0/glib/gatomic.h
math.o: /usr/include/glib-2.0/glib/gbacktrace.h /usr/include/signal.h
math.o: /usr/include/bits/sigset.h /usr/include/glib-2.0/glib/gbase64.h
math.o: /usr/include/glib-2.0/glib/gbitlock.h
math.o: /usr/include/glib-2.0/glib/gbookmarkfile.h /usr/include/time.h
math.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
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
math.o: /usr/include/glib-2.0/glib/gvariant.h kernel.h /usr/include/stdlib.h
math.o: /usr/include/bits/waitflags.h /usr/include/bits/waitstatus.h
math.o: /usr/include/endian.h /usr/include/bits/endian.h
math.o: /usr/include/bits/byteswap.h /usr/include/xlocale.h
math.o: /usr/include/sys/types.h /usr/include/sys/select.h
math.o: /usr/include/bits/select.h /usr/include/bits/time.h
math.o: /usr/include/sys/sysmacros.h /usr/include/bits/pthreadtypes.h
math.o: /usr/include/alloca.h /usr/include/inttypes.h /usr/include/stdint.h
math.o: /usr/include/bits/wchar.h /usr/include/glib-2.0/gmodule.h
math.o: aul/include/aul/common.h /usr/include/string.h aul/include/aul/log.h
math.o: aul/include/aul/error.h aul/include/aul/string.h
math.o: aul/include/aul/list.h kernel-priv.h /usr/include/sched.h
math.o: /usr/include/bits/sched.h
