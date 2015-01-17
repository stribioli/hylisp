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

#include "mux.h"

#include <pthread.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#ifdef LINUX_OS

/* From OpenLISP <net/lisp/maptables.h> */
#define MAPF_DB       0x001
#define MAPM_ADD	   0x01
#define MAPM_DELETE	   0x02

#else 	/* LINUX_OS */

#include <net/lisp/lisp.h>
#include <net/lisp/maptables.h>

#endif	/* LINUX_OS */

#include "../common/common.h"
#include "../controlplanes/assignments.h"
#include "../controlplanes/connected.h"
#include "parsemessage.h"

#ifndef INFTIM
#define INFTIM	-1
#endif

static void init_mmm_poll();
static void expand_mmm_poll();
static void update_mmm_poll();
static void process_message();

static const int DYN_ARRAY_INIT = 16;
static const double DYN_ARRAY_INCR = 1.5;

struct pollfd* mmm_poll = NULL;
int mmm_poll_ctr = 0;
int mmm_poll_max = 0;
int map_socket = -1;

void* start_mapmessages_muxer(void* arg) {
	map_socket = socket(PF_MAP, SOCK_RAW, 0);
	
	if (map_socket < 0) {
		fatal("Unable to open mapping socket");
	}
	
	init_mmm_poll();

	debug_printf("Map message muxer is listening");
	
	int r;
	while (1) {
		r = poll(mmm_poll, mmm_poll_ctr, INFTIM);

		if (r == -1) {
			fatal("Error while polling in map messages muxer");
		}

		if (mmm_poll[0].revents & POLLIN) {
			/* Registering_server thread signalled a change */
			update_mmm_poll();

			if (r == 1) {
				/* No other socket is ready */
				continue;
			}
		}

		for (int i = 1; i < mmm_poll_ctr; ++i) {
			if (mmm_poll[i].revents & POLLIN) {
				process_message(mmm_poll[i].fd);
			}
		}
	}

	pthread_exit(0);
}

static void init_mmm_poll() {
	mmm_poll = malloc(DYN_ARRAY_INIT * sizeof(struct pollfd));
	check_allocation(mmm_poll);

	mmm_poll_ctr = 0;
	mmm_poll_max = DYN_ARRAY_INIT;

	int s = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		fatal("Unable to create internal UNIX socket");
	}

	fcntl(s, F_SETFL, O_NONBLOCK);

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/var/hylisphv/sockets/poll_update");
	unlink(addr.sun_path);

	int addr_len;
	addr_len = sizeof(struct sockaddr_un);

	int r;
	r = bind(s, (struct sockaddr*) &addr, addr_len);
	if (r < 0) {
		fatal("Unable to bind internal UNIX socket");
	}

	mmm_poll[0].fd = s;
	mmm_poll[0].events = POLLIN;
	mmm_poll[0].revents = 0x0;

	mmm_poll_ctr = 1;
}

static void expand_mmm_poll(int guaranteed_capacity) {
	mmm_poll_max = (int) ceil(DYN_ARRAY_INCR * mmm_poll_max);

	if (guaranteed_capacity > 0 && mmm_poll_max < guaranteed_capacity) {
		mmm_poll_max = guaranteed_capacity;
	}

	mmm_poll = realloc(mmm_poll, mmm_poll_max * sizeof(struct pollfd));
	check_allocation(mmm_poll);
}

static void update_mmm_poll() {
	const int INT_BUF_LEN = 8;

	uint8_t buf[INT_BUF_LEN];

	pthread_rwlock_rdlock(&control_planes_lock);

	int pre_update_ctr = mmm_poll_ctr-1;
	
	int r = 1;
	while (r > 0) {
		r = recv(mmm_poll[0].fd, buf, INT_BUF_LEN, 0);
	}

	if (control_planes_ctr > mmm_poll_max - 1) {
		expand_mmm_poll(control_planes_ctr + 1);
	}

	mmm_poll_ctr = control_planes_ctr + 1;

	for (int i = 0; i < control_planes_ctr; i++) {
		mmm_poll[i + 1].fd = control_planes[i].socket_descriptor;
		mmm_poll[i + 1].events = POLLIN;
		mmm_poll[i + 1].revents = 0x0;
	}
	
	debug_printf("Updating MapMessagesMuxer. Currently %d control planes are in the pool (they were %d)", mmm_poll_ctr-1, pre_update_ctr);

	pthread_rwlock_unlock(&control_planes_lock);
}

static void process_message(int socket_descriptor) {
	const int MAP_BUF_LEN = 8192;

	int msg_len;
	uint8_t buf[MAP_BUF_LEN];

	memset(buf, 0, MAP_BUF_LEN);
	msg_len = recv(socket_descriptor, buf, MAP_BUF_LEN, 0);

	if (msg_len < 0) {
		warning("Error while reading from northbound socket");
		return;
	}

	uint16_t msg_type = extract_type_mm(buf, msg_len);
	uint32_t msg_flags = extract_flags_mm(buf, msg_len);
	
	/* Add to/remove from assignments if EID is local */
	if ((msg_flags & MAPF_DB) && (msg_type == MAPM_ADD || msg_type == MAPM_DELETE)) {
		pthread_rwlock_rdlock(&control_planes_lock);
		pthread_rwlock_rdlock(&assignments_lock);

		ipv6_prefix significant_eid;
		significant_eid = extract_eid_mm(buf, msg_len);
		
		assignment item;
		memcpy(&(item.eid), &(significant_eid), sizeof(ipv6_prefix));
		
		int i;
		for (i = 0; i < control_planes_ctr; ++i) {
			if (control_planes[i].socket_descriptor == socket_descriptor) {
				break;
			}
		}
		
		item.assignee_index = i;
		
		if (i < control_planes_ctr) {
			switch (msg_type) {
				case MAPM_ADD:
					add_assignment(&item);
					break;
				case MAPM_DELETE:
					remove_assignment(&item);
					break;
			}
		}
		
		pthread_rwlock_unlock(&assignments_lock);
		pthread_rwlock_unlock(&control_planes_lock);
	}
		
	int r = write(map_socket, buf, msg_len);

	if (r == -1) {
		r = errno;

		switch (errno) {
		case EINVAL:
		case ESRCH:
		case EBUSY:
		case ENOBUFS:
		case EEXIST:
			break;
		default:
			warning("Unable to write to mapping socket");
			break;
		}
	} else {
		r = 0;
	}

	debug_printf("Muxing map message of type %d from northbound socket %d to data plane", msg_type, socket_descriptor);
	if (r != 0) {
		debug_printf("However, map socket returned errno %d", r);
	}
}
