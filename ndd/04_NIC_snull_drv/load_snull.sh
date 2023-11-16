#!/bin/sh
DRV=snull
export PATH=/sbin:/bin:$PATH

# remove any stale driver instance
lsmod|grep $DRV >/dev/null && rmmod $DRV
make || exit 1
dmesg -c
sync

# Use a pathname, as new modutils don't look in the current dir by default
insmod ./$DRV.ko $*

ifconfig sn0 10.10.0.1 netmask 255.255.255.0
ifconfig sn1 10.10.1.1 netmask 255.255.255.0
