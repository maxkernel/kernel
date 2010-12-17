#!/bin/bash

if [ $UID != 0 ];
then
	echo "Install script must be run as root!"
	exit 0
fi

echo -n "=====****----.... MaxRobot software interactive-installer ....----****=====
* Assumes that OS is a fresh copy of Ubuntu Server 9.10
* Installer must be run as root and have internet access
*
* Shall I continue? [Y/n] "
read response

if [ "$response" = "n" ] || [ "$response" = "N" ];
then
	echo "Abort."
	echo 1
fi


INSTALL=/etc/maxos

#update system
apt-get update
apt-get -y upgrade

#install dependancies
DEP="
linux-rt
acpid
build-essential
xutils-dev
yasm
gdb
openssh-server
pkg-config
binutils-dev
texlive
libglib2.0-dev
libgnet-dev
libconfuse-dev
libmatheval1-dev
libmicrohttpd-dev
libjpeg-dev
libffi-dev
libv4l-dev
libdc1394-22-dev
zlib1g-dev
libtar-dev
sqlite
libsqlite-dev
sun-java6-jdk
lua5.1
liblua5.1-dev
liblua5.1-socket-dev
zip
unzip
"
apt-get -y install $DEP

#install script files
mkdir -p $INSTALL
cp -f config $INSTALL
cat max-network | sed "s|\(^INSTALL=\)\(.*\)$|\1$INSTALL|" >$INSTALL/max-network
cat max-chgname | sed "s|\(^INSTALL=\)\(.*\)$|\1$INSTALL|" >$INSTALL/max-chgname
cat maxos.initd | sed "s|\(^INSTALL=\)\(.*\)$|\1$INSTALL|" >/etc/init.d/maxos

#make things executable
chmod +x $INSTALL/max-network
chmod +x $INSTALL/max-chgname
chmod +x /etc/init.d/maxos

#create sym links
ln -sf $INSTALL/max-network /usr/sbin
ln -sf $INSTALL/max-chgname /usr/sbin

#update rc
update-rc.d -f maxos remove
update-rc.d maxos start 90 2 3 4 5 . stop 90 0 1 6 .


#call the util scripts
echo -n "*************************************************************************
* What is the SSID for the wireless network? "
read essid
echo -n "* What is the key? (enter the hex key, 'off', or s: for a string key) "
read key
echo -n "* What do you want to name the robot? "
read name

max-network $essid $key
max-chgname $name

echo "NEXT STEP: Install the packaged dependencies in the contrib directory."
echo "Installer done."

