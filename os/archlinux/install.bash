#!/bin/bash

echo -n "=====****----.... MaxRobot software interactive-installer ....----****=====
* Assumes that OS is a fresh copy of Archlinux
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

"
pacman -Sy --noconfirm $DEP

echo "Installer done."

