#!/bin/bash

echo -n "=====****----.... MaxRobot software interactive-installer ....----****=====
* Assumes that OS is a fresh copy of Debian (Wheezy/Ubuntu Server/etc.)
* Installer must be run as root and have internet access
*
* Shall I continue? [Y/n] "
read response

if [ "$response" = "n" ] || [ "$response" = "N" ];
then
	echo "Abort."
	echo 1
fi

if [ $UID != 0 ];
then
	echo "Install script must be run as root!"
	exit 0
fi

#install dependancies
DEP="
build-essential
xutils-dev
binutils-dev
pkg-config
git-core
gdb
libglib2.0-dev
libconfuse-dev
libjpeg-dev
libffi-dev
libv4l-dev
zlib1g-dev
libtar-dev
sqlite3
libsqlite3-dev
lua5.1
liblua5.1-dev
liblua5.1-socket-dev
"
apt-get -y install $DEP

echo "Installer done."

