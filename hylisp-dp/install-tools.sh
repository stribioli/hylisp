#!/bin/sh -x

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
patch -d /usr/src -u -p1 < ./tools.patch
