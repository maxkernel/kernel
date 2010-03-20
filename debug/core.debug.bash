#!/bin/bash

# param 1 = <pid>
# param 2 = <file>
# param 3 = <title>

# ensure parent directory exists
parent_regex='(/.*)/[A-Za-z0-9\\.\\-]*'
if [[ $2 =~ $parent_regex ]]; then
	mkdir -p "${BASH_REMATCH[1]}"
fi

# print header
echo "$3" > $2
echo "Time: `date`" >> $2
echo "===================================" >> $2

# build scripts that get executed on maxkernel
tfile=`tempfile`
echo "attach $1" >> $tfile
echo "thread apply all backtrace" >> $tfile

# now execute them
echo -e "\n\nStack trace::" >> $2
gdb -batch -x $tfile 2>&1 >> $2

# print out all open files
echo -e "\n\nOpen files::" >> $2
lsof -p $1 2>>/dev/null >> $2

# clean up
rm -f $tfile
