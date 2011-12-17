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

OBJS		= \$(SRCS:.c=.o)
TARGET		= $module.mo
DEFINES		+= $defines -DMODULE
INCLUDES	+= -Iinclude -I.. -I../aul/include \$(foreach dep,\$(DEPENDS),-I../\$(dep)/include)
LIBS		+= \$(shell pkg-config --libs glib-2.0) \$(shell [ -n "\$(PACKAGES)" ] && pkg-config --libs \$(PACKAGES))

CFLAGS		= -pipe -ggdb3 -Wall -std=gnu99 -fpic \$(shell pkg-config --cflags glib-2.0) \$(shell [ -n "\$(PACKAGES)" ] && pkg-config --cflags \$(PACKAGES))
LFLAGS		= -shared


.PHONY: install clean depend

all: \$(TARGET)

\$(TARGET): \$(OBJS)
	\$(CC) \$(CFLAGS) \$(DEFINES) \$(INCLUDES) -o \$(TARGET) \$(OBJS) \$(LFLAGS) \$(LIBS)
	[ ! -f compile.part.bash ] || bash compile.part.bash

install:
	[ -n "\$(INSTALL)" ] || ( echo "INSTALL variable must be set" 1>&2 && false )
	cp -f $module.mo "\$(INSTALL)/modules"
	( cp -f include/* /usr/include/max 2>/dev/null ) || true
	[ ! -f install.part.bash ] || bash install.part.bash \$(INSTALL)

clean:
	rm -f \$(TARGET) \$(OBJS)
	( [ -f clean.part.bash ] && bash clean.part.bash ) || true

depend: \$(SRCS)
	makedepend \$(DEFINES) \$(INCLUDES) \$^ 2>/dev/null
	rm -f Makefile.bak

.c.o:
	\$(CC) \$(CFLAGS) \$(DEFINES) \$(INCLUDES) -c \$< -o \$@

# DO NOT DELETE THIS LINE -- make depend needs it
END
