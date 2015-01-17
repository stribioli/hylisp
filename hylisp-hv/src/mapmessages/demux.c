/*
Copyright (c) 2014, Stefano Tribioli
All rights reserved. 

This file is part of the hyLISP project.
http://hylisp.org

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met: 
1. Redistributions of source code must retain the above copyright notice, this 
   list of conditions and the following disclaimer. 
2. Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation 
   and/or other materials provided with the distribution. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR 
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "demux.h"
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#ifdef LINUX_OS

/* From OpenLISP <net/lisp/maptables.h> */
struct map_msghdr {
	uint8_t map_msglen;
	uint8_t map_version;
	uint16_t map_type;
	uint32_t map_flags;
	uint16_t map_addrs;
	uint16_t map_versioning;
	int map_rloc_count;
	pid_t map_pid;
	int map_seq;
	int map_errno;
};

int sysctlbyname(const char *name, void *oldp, size_t *oldlenp, const void *newp, size_t newlen) {
	return 0;	
}

#else	/* LINUX_OS */

#include <net/lisp/lisp.h>
#include <net/lisp/maptables.h>

#endif	/* LINUX_OS */

#include "../common/common.h"
#include "parsemessage.h"

int map_socket;

static void process_message(uint8_t*, int);

void* start_mapmessages_demuxer(void* arg) {
	char* sysctl_val = "header";
	int r = sysctlbyname("net.lisp.missmsg", NULL, NULL, sysctl_val, strlen(sysctl_val));
	if (r < 0) {
		fatal("Unable to set sysctl net.lisp.missmsg");
	}
	
	while (map_socket == -1) {
		struct timespec rqtp = {0, 1000000};
		nanosleep(&rqtp, NULL);
	}

	const int MAP_BUF_LEN = 8192;

	int l;
	uint8_t buf[MAP_BUF_LEN];

	debug_printf("Map message demuxer is listening");

	while (1) {
		l = read(map_socket, buf, MAP_BUF_LEN);
		process_message(buf, l);
	}

	pthread_exit(0);
}

static void process_message(uint8_t* buf, int msg_len) {
	int r;

	int recipient_socket;

	ipv6_prefix significant_eid;
	significant_eid = extract_eid_mm(buf, msg_len);

	pthread_rwlock_rdlock(&control_planes_lock);
	pthread_rwlock_rdlock(&assignments_lock);

	int control_plane_index;
	r = sort(&significant_eid, &control_plane_index);

	if (r == SORTING_ONE) {
		recipient_socket = control_planes[control_plane_index].socket_descriptor;
	}

	pthread_rwlock_unlock(&assignments_lock);
	pthread_rwlock_unlock(&control_planes_lock);

	if (r == SORTING_ONE) {
		r = write(recipient_socket, buf, msg_len);

		if (r == -1) {
			warning("Unable to write to UNIX socket");
			return;
		}

		debug_printf("Demuxing map message (type %u) from data plane to northbound socket %d", ((struct map_msghdr*) buf)->map_type, recipient_socket);
		debug_printf_prefix(&significant_eid);
	} else {
		debug_printf("Unable to demux map message (type %u; result 0x%x), broadcasting to all control planes", ((struct map_msghdr*) buf)->map_type, r);
		debug_printf_prefix(&significant_eid);

		/* Broadcasting map message */

		pthread_rwlock_rdlock(&control_planes_lock);

		for (int i = 0; i < control_planes_ctr; i++) {
			r = write(control_planes[i].socket_descriptor, buf, msg_len);

			if (r == -1) {
				warning("Unable to write to UNIX socket");
			}
		}

		pthread_rwlock_unlock(&control_planes_lock);
	}
}
