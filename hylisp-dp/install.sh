#!/bin/sh

if [ ! -d /usr/src/sys ]
then
	echo "Source tree is not available"
	exit 1
fi

TargetRelease="10.1-RELEASE"
if [ $(freebsd-version) != $TargetRelease ]
then
	echo "This patch is for FreeBSD $TargetRelease, but you have FreeBSD $(freebsd-version)"
	exit 1
fi

# Patching source tree
patch -d /usr/src -u -p1 < ./core.patch

# Copying patched headers
cp /usr/src/sys/sys/socket.h /usr/include/sys/socket.h
cp /usr/src/sys/sys/mbuf.h /usr/include/sys/mbuf.h
cp /usr/src/sys/net/netisr.h /usr/include/net/netisr.h
chmod 0444 /usr/include/sys/socket.h /usr/include/sys/mbuf.h /usr/include/net/netisr.h

# Copying new headers
mkdir -p /usr/include/net/lisp
cp /usr/src/sys/net/lisp/lisp.h /usr/include/net/lisp/lisp.h
cp /usr/src/sys/net/lisp/maptables.h /usr/include/net/lisp/maptables.h
cp /usr/src/sys/net/lisp/maptables_xpg.h /usr/include/net/lisp/maptables_xpg.h
chmod 0444 /usr/include/net/lisp/lisp.h /usr/include/net/lisp/maptables.h /usr/include/net/lisp/maptables_xpg.h

# Copying kernel configuration
Arch=$(uname -m)
if [ ! -d /usr/src/sys/$Arch/conf ]
then
	echo "Unable to get machine architecture. You have to generate a kernel configuration by hand."
else
	cp -n ./LISP.kernconf /usr/src/sys/$Arch/conf/LISP
	chmod 0644 /usr/src/sys/$Arch/conf/LISP
fi
