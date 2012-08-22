ANT		= /usr/bin/ant
DOXYGEN		= /usr/bin/doxygen


TEST_TARGET	= Service

.PHONY: prereq all install clean rebuid docs

all:
	$(ANT) -f build.xml all

test:
	$(ANT) -f build.xml test -Dtest=$(TEST_TARGET)

install:

clean:
	$(ANT) -f build.xml clean
	rm -rf doxygen

rebuild: clean all

docs:
	mkdir -p doxygen
	$(DOXYGEN) Doxyfile

