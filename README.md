# hyLISP

hyLISP is an open source implementation of a LISP edge router (*xTR*) running on FreeBSD.

## Overview

This project is a bundle of three interoperating but clearly distinct layers:

* Data plane
* Hypervisor (hence the name hyLISP)
* Control plane

### Data plane (*hylisp-dp*)

The data plane is integrated in FreeBSD kernel (a kernel recompilation is thus required).

This layer is a porting of Luigi Iannone's [OpenLISP project](http://openlisp.org) to the last FreeBSD release (currently 10.1) and is released under a 3-clause BSD license.

The porting was necessary since OpenLISP was abandoned after FreeBSD 8.2. An [alternative port](https://github.com/lip6-lisp/data-plane), written by LIP6, is up to date, but I found it to be severely unreliable.

### Hypervisor (*hylisp-hv*)

The hypervisor allows multiple control plane processes to easily interface with the data plane.

Currently, messages are muxed and demuxed between data and control planes on a per-EID basis; however, the hypervisor could (and hopefully will) be extended to handle more complex scenarios and even support the migration of control planes to different machines.

This layer is original work and is released under a 2-clause BSD license.

### Control plane (*hylisp-cp*)

The control plane handles the exchange of LISP control packets, chiefly EID registrations, cache miss resolutions, and map replies.

Currently, this layer is a minor adaptation of [LIP6's LISP control plane](https://github.com/lip6-lisp/control-plane/tree/master/lisp-control-plane-freebsd_v3.2b), also confusingly known under the *OpenLISP* name. The code from LIP6 states a modified 3-clause BSD license, but since it includes files from the GPLed GNU Zebra, it definitely falls under the GPL as well.

LIP6 code does a good job, but its architecture is quite intricate and it supports roles of no interest for this project. Therefore, a new, much simpler control plane will hopefully be written in the future.

## Usage

### Data plane

The data plane is part of the kernel and does not require to be launched. You can tune its functions using the original *OpenLISP* `sysctl` options, documented hereunder.

* `net.lisp.etr` The behavior of the machine when decapsulating LISP packets.
	* `standard` (default) Packets are always decapsulated and forwarded. This is in accordance with LISP specifications.
	* `notify` As before. However, if no cache entry is available for the source EID, a *MISS* message is sent to the upper layers.
	* `secure` If no cache entry is available for the source EID, a *MISS* message is sent and the packet is dropped.

* `net.lisp.missmsg` The type of *MISS* message sent upwards.
	* `ip` The message is preceded by the destination EID in a `struct sockaddr`. Please note that this is the OpenLISP default, but it is changed to `header` as soon as the hypervisor is launched.
	* `header` (default) The message is preceded by the IP header of the packet that generated the miss. 
	* `packet` The message is preceded by the entire packet that generated the miss.

* `net.lisp.srcport` The strategy to take when choosing the source port for LISP encapsulated packets.
	* `lispdata` (default) Always use port 4341.
	* `shorthash` The source port number is obtained from a hash function. For IPv4, the source IP address, the destination IP address, and the Protocol Number of the IP header of the original packet are used. In case of IPv6, the source IP address, the destination IP address, and the Next Header of the IP header of the original packet are used.
	* `longhash` The source port number is obtained from a hash function. The used fields are the same like in the shorthash case with in addition the first 4 bytes right after the IP header of the original packet. Note that this are usually the bytes that hold the source and destination ports for protocols like UDP, TCP, and SCTP, however, there is no check if it is actually the case. The algorithm blindly uses the first for 4 bytes right after the IP header.
	* `adaptivehash` The source port number is obtained from a hash function. The same algorithm as longhash is performed if the header after the IP header is UDP, TCP, or SCTP, otherwise shorthash is used. In other words, the 4 bytes right after the IP header are used only if they actually hold source and destination port numbers.
 
* `net.lisp.hashseed` An integer value used as a seed in the hash function used to calculate the source port number of the LISP encapsulated packet. The hash function used is based on the [code](http://burtleburtle.net/bob/c/lookup3.c) developed by Bob Jenkins.

* `net.lisp.debug` Disables (set to 0) or enables (any other value) log messages. They are written to `/var/log/debug.log`

* `net.lisp.xpgtimer` The period (in seconds) between two cache sweeps. When the timer fires, all the cache entries that have not been used in the last `net.lisp.xpgtimer` seconds are removed. This value must be between 60 (one minut) and 86400 (one day). Otherwise, an `off` value can be used to turn off cache sweeping.

* `net.lisp.netisr_maxqlen` The system maximum dispatch queue length for Mapping Sockets.

### Hypervisor

Start the hypervisor with `hylisphv`. The following options are provided:

* `-d` Start the hypervisor as a daemon. You should use it only when using the program as a service (see just below).

* `-o` or `-o <file>` or `-O <file>` Enable debug messages. In the first case, write to `stdout`; in the second, write to `<file>` after truncating it; in the third, append to `<file>`. If debug is active and a fatal error occurs, the core is dumped.

* `-e <file>` or `-E <file>` In the first case, write error messages to `<file>` after truncating it; in the second, append them to `<file>`. If neither is provided, writes to `stderr`.

A service is provided. See INSTALL.md for details on how to install it. By default, it writes to `/var/hylisphv/debug.log` and `/var/hylisphv/error.log` files. However, you can edit the `/etc/rc.d/hylisphv` script to suit your needs.

### Control plane

As said, the control plane is currently based on *LIP6* work. Their control plane requires a `.conf` and a `.xml` file. You can use the examples provided and adapt them to your needs. Visit [their GitHub page](https://github.com/lip6-lisp/control-plane/tree/master/lisp-control-plane-freebsd_v3.2b/doc) for more information.

When done, you can launch it with `hylispcp -v -f /etc/hylispcp/<filename>.conf`. Note the `-v` flag that makes the control plane aware of the hypervisor layer.