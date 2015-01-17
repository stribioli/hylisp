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

#include "register.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "../common/common.h"
#include "assignments.h"
#include "connected.h"

typedef struct {
	int action;
	struct sockaddr_un socket;
	uint16_t port;
	pid_t pid;
} hv_registration_message;

#define ACTION_REGISTER		0
#define ACTION_DEREGISTER	1

int ipv4_controlpackets_socket;
int ipv6_controlpackets_socket;

static void register_control_plane(hv_registration_message*);
static void deregister_control_plane(hv_registration_message*);
static void ping_mmm();
static void ack_registration(int);

static int s_ping_is_connected = 0;
static int s_ping = -1;

void* start_registering_server(void* arg) {
	const int REG_BUF_LEN = 200;

	init_control_planes();
	init_assignments();

	int s = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		fatal("Unable to create eastbound socket");
	}
	
	system("rm -f /var/hylisphv/sockets/cp_*");

	struct sockaddr_un addr_nr;
	addr_nr.sun_family = AF_UNIX;
	strcpy(addr_nr.sun_path, "/var/hylisphv/sockets/register");
	unlink(addr_nr.sun_path);

	int addr_nr_len;
	addr_nr_len = sizeof(struct sockaddr_un);

	int r;
	r = bind(s, (struct sockaddr*) &addr_nr, addr_nr_len);
	if (r < 0) {
		fatal("Unable to bind eastbound socket");
	}
	r = chmod(addr_nr.sun_path, S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH);
	if (r < 0) {
		fatal("Unable to set permissions on eastbound socket");
	}

	s_ping = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (s_ping < 0) {
		fatal("Unable to create internal ping socket");
	}

	debug_printf("Eastbound server is listening");

	int msg_len;
	uint8_t buf[REG_BUF_LEN];
	while (1) {
		memset(buf, 0, REG_BUF_LEN);
		msg_len = recv(s, buf, REG_BUF_LEN, 0);

		if (msg_len < 0) {
			fatal("Error while reading from eastbound socket");
		}

		if (msg_len == sizeof(hv_registration_message)) {
			hv_registration_message* msg;
			msg = (hv_registration_message*) buf;

			switch (msg->action) {
			case ACTION_REGISTER:
				register_control_plane(msg);
				break;
			case ACTION_DEREGISTER:
				deregister_control_plane(msg);
				break;
			default:
				debug_printf("Unknown action (%d) sent to eastbound server", msg->action);
				continue;
			}
		} else {
			debug_printf("Message of wrong length (%d) sent to eastbound server", msg_len);
		}
	}
}

static void register_control_plane(hv_registration_message* msg) {
	int r;

	pthread_rwlock_wrlock(&control_planes_lock);

	control_plane cp;
	cp.id = control_planes_autoindex++;

	int s;
	s = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		fatal("Unable to create a northbound socket");
	}

	struct sockaddr_un local_addr;
	local_addr.sun_family = AF_UNIX;
	strcpy(local_addr.sun_path, msg->socket.sun_path);
	strcat(local_addr.sun_path, "_r");

	r = bind(s, (struct sockaddr*) &local_addr, sizeof(struct sockaddr_un));
	if (r < 0) {
		fatal("Unable to bind a northbound socket");
	}
	r = chmod(local_addr.sun_path, S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH);
	if (r < 0) {
		fatal("Unable to set permissions on a northbound socket");
	}

	r = connect(s, (struct sockaddr *) &(msg->socket), sizeof(struct sockaddr_un));
	if (r < 0) {
		fatal("Unable to connect to a northbound socket");
	}

	cp.socket_descriptor = s;
	cp.port = msg->port;
	cp.pid = msg->pid;
	
	add_control_plane(&cp);

	pthread_rwlock_unlock(&control_planes_lock);

	debug_printf("Control plane %d registered; northbound socket %d; UDPv4 port %d", cp.id, s, cp.port);

	ack_registration(s);

	/* Pinging  MapMessagesMuxer to force update */
	ping_mmm();
}

static void deregister_control_plane(hv_registration_message* msg) {
	pthread_rwlock_wrlock(&control_planes_lock);

	/* Finding control plane */
	int i;
	for (i = 0; i < control_planes_ctr; i++) {
		if (control_planes[i].port == msg->port) {
			break;
		}
	}

	uint32_t id;
	if (i < control_planes_ctr) {
		id = remove_control_plane(i);

		debug_printf("Control plane %d deregistered", id);
	} else {
		debug_printf("Control plane asked to be deregistered, but port %d is not registered", msg->port);
	}

	pthread_rwlock_unlock(&control_planes_lock);

	pthread_rwlock_wrlock(&assignments_lock);

	if (i < control_planes_ctr) {
		remove_assignee(id);
	}

	pthread_rwlock_unlock(&assignments_lock);

	ping_mmm();
}

static void ping_mmm() {
	int r;

	if (!s_ping_is_connected) {
		struct sockaddr_un addr_pu;
		addr_pu.sun_family = AF_UNIX;
		strcpy(addr_pu.sun_path, "/var/hylisphv/sockets/poll_update");

		int addr_pu_len;
		addr_pu_len = sizeof(struct sockaddr_un);

		r = connect(s_ping, (struct sockaddr *) &addr_pu, addr_pu_len);
		if (r < 0) {
			fatal("Unable to connect to internal ping socket");
		}

		s_ping_is_connected++;
	}

	const char* PING_MSG = "UPDATE";

	r = write(s_ping, PING_MSG, strlen(PING_MSG));
	if (r < 0) {
		warning("Unable to write to internal ping socket");
	}
}

static void ack_registration(int s) {
	char buf[5] = "ACK";

	struct iovec iov[1];
	iov[0].iov_base = buf;
	iov[0].iov_len = strlen(buf);

	struct msghdr msg;
	struct cmsghdr *cmptr;

	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	int number_of_sockets = 1;
	if (ipv6_controlpackets_socket >= 0) {
		number_of_sockets = 2;
	}

	cmptr = malloc(CMSG_SPACE(2 * sizeof(int)));
	check_allocation(cmptr);
	cmptr->cmsg_level = SOL_SOCKET;
	cmptr->cmsg_type = SCM_RIGHTS;
	cmptr->cmsg_len = CMSG_LEN(number_of_sockets * sizeof(int));
	int* cmsg_data = (int*) CMSG_DATA(cmptr);
	cmsg_data[0] = ipv4_controlpackets_socket;
	cmsg_data[1] = ipv6_controlpackets_socket;

	msg.msg_control = (caddr_t) cmptr;
	msg.msg_controllen = CMSG_SPACE(number_of_sockets * sizeof(int));

	int r;
	r = sendmsg(s, &msg, 0);
	if (r < 0) {
		free(cmptr);
		fatal("Unable to pass file descriptors via a northbound socket");
	}

	free(cmptr);
}
