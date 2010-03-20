#!/usr/bin/perl

use Getopt::Long;

#parse options
GetOptions( "module=s" => \$module,
	    "defines=s" => \$defines );


#print build.cfg file
open( FILE, "< $module/build.cfg" );
while( <FILE> ) {
	print STDOUT;
}
close( FILE );

#print rest of makefile
print STDOUT <<END;
TARGET		= $module.mo
DEFINES		= $defines
LINKFLAGS	= \$(shell pkg-config --libs glib-2.0) \$(shell [ -n "\$(PACKAGES)" ] && pkg-config --libs \$(PACKAGES)) \$(LINKOPTS)
COMPILEFLAGS	= -pipe -ggdb3 -Wall -Iinclude -I.. -I../aul/include \$(DEFINES) -DMODULE \$(COMPILEOPTS) -fpic \$(shell pkg-config --cflags glib-2.0) \$(shell [ -n "\$(PACKAGES)" ] && pkg-config --cflags \$(PACKAGES))


all: \$(OBJECTS)
	\$(CC) -o \$(TARGET) \$(OBJECTS) -shared \$(LINKFLAGS)
	[ ! -f compile.part.bash ] || bash compile.part.bash
	rm -f \$(OBJECTS)

clean:
	rm -f \$(TARGET) \$(OBJECTS)
	( [ -f clean.part.bash ] && bash clean.part.bash ) || true

install:
	[ -n "\$(INSTALL)" ] || ( echo "INSTALL variable must be set" 1>&2 && false )
	cp -f $module.mo "\$(INSTALL)/modules"
	( cp -f include/* /usr/include/max 2>/dev/null ) || true
	[ ! -f install.part.bash ] || bash install.part.bash \$(INSTALL)

%.o: %.c
	\$(CC) -c \$(COMPILEFLAGS) \$*.c -o \$*.o

END
