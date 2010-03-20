#!/bin/bash

if [ $UID != 0 ];
then
	echo "Install script must be run as root!"
	exit 0
fi

echo "=====****----.... MaxRobot contrib automatic-installer ....----****=====
* Assumes that OS is a fresh copy of Ubuntu Server 9.10 with the
* interactive installer already executed"

echo "== Installing x264 video library (nightly build Feb 16 2010)"
tar -xf x264-snapshot-20100216-2245.tar.bz2
cd x264-snapshot-20100216-2245/
./configure --enable-debug --enable-shared --enable-pic 1>/dev/null
make 1>/dev/null && make install 1>/dev/null
cd ../


echo "== Post installion tasks"
ldconfig

echo "== Cleaning up"
rm -rf x264-snapshot-20100216-2245

echo "Install completed. Have a nice day."
