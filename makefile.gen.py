#!/usr/bin/python

import sys, getopt

try:
    opts, args = getopt.getopt(sys.argv[1:], '', ['module=','defines='])
    opts = dict(opts)
except getopt.GetoptError, err:
    print >> sys.stderr, 'Could not parse command line options.'
    sys.exit(1)

if '--module' not in opts:
    print >> sys.stderr, 'Module parameter must be defined.'
    sys.exit(1)

if '--defines' not in opts:
    opts['--defines'] = ''

filename = '%s/build.cfg' % (opts['--module'])
with open(filename) as fp:
    print >> sys.stdout, fp.read(),


print >> sys.stdout, '''
OBJS		= $(SRCS:.c=.o)
TARGET		= %(module)s.mo
DEFINES		+= %(defines)s -DMODULE
INCLUDES	+= -Iinclude -I.. -I../aul/include $(foreach dep,$(DEPENDS),-I../$(dep)/include)
LIBS		+= $(shell pkg-config --libs glib-2.0) $(shell [ -n "$(PACKAGES)" ] && pkg-config --libs $(PACKAGES))

CFLAGS		= -pipe -ggdb3 -Wall -std=gnu99 -fpic $(shell pkg-config --cflags glib-2.0) $(shell [ -n "$(PACKAGES)" ] && pkg-config --cflags $(PACKAGES))
LFLAGS		= -shared


.PHONY: install clean depend

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)
	[ ! -f compile.part.bash ] || bash compile.part.bash

install:
	[ -n "$(INSTALL)" ] || ( echo "INSTALL variable must be set" 1>&2 && false )
	cp -f $(TARGET) $(INSTALL)/modules
	( cp -f include/* /usr/include/max 2>/dev/null ) || true
	[ ! -f install.part.bash ] || bash install.part.bash $(INSTALL)

clean:
	rm -f $(TARGET) $(OBJS)
	( [ -f clean.part.bash ] && bash clean.part.bash ) || true

depend: $(SRCS)
	makedepend $(DEFINES) $(INCLUDES) $^ 2>/dev/null
	rm -f Makefile.bak

.c.o:
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@

# DO NOT DELETE THIS LINE -- make depend needs it
''' % {
'module'    :   opts['--module'],
'defines'   :   opts['--defines'],
},

