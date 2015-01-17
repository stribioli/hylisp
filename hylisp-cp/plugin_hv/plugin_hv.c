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

#include "plugin_hv.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <hylisphv.h>

#include "../lib.h"

int _virtual = 0;
uint16_t virtual_udp_port = 0;
int virtual_map_socket = -1;
int ipv4_hv_socket = -1;
int ipv6_hv_socket = -1;

void register_virtual_plane() {
	int r;

	/* Creating virtual Map socket */

	virtual_map_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (virtual_map_socket < 0) {
		perror("socket");
		exit(0);
	}

	struct sockaddr_un local_addr;
	local_addr.sun_family = AF_UNIX;
	strcpy(local_addr.sun_path, "/var/hylisphv/sockets/cp_XXXXXX");

	// TODO: find a better way of generating random filenames
	r = mkstemp(local_addr.sun_path);
	unlink(local_addr.sun_path);

	struct sockaddr_un remote_addr;
	remote_addr.sun_family = AF_UNIX;
	strcpy(remote_addr.sun_path, local_addr.sun_path);
	strcat(remote_addr.sun_path, "_r");

	r = bind(virtual_map_socket, (struct sockaddr*) &local_addr, sizeof(struct sockaddr_un));
	if (r < 0) {
		perror("bind");
		exit(0);
	}
	r = chmod(local_addr.sun_path, S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH);

	/* Registering */

	hv_registration_message reg_msg;

	reg_msg.action = ACTION_REGISTER;
	memcpy(&(reg_msg.socket), &local_addr, sizeof(struct sockaddr_un));
	reg_msg.port = virtual_udp_port;
	reg_msg.pid = getpid();

	int reg_s = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (reg_s < 0) {
		perror("socket");
		exit(0);
	}

	struct sockaddr_un reg_addr;
	reg_addr.sun_family = AF_UNIX;
	strcpy(reg_addr.sun_path, "/var/hylisphv/sockets/register");

	r = connect(reg_s, (struct sockaddr *) &reg_addr, sizeof(struct sockaddr_un));
	if (r < 0) {
		perror("connect");
		exit(0);
	}

	r = write(reg_s, (char *) &(reg_msg), sizeof(reg_msg));
	if (r < 0) {
		perror("write");
		exit(0);
	}

	close(reg_s);

	/* Waiting for ACK to get remote endpoint and socket descriptors */
	char buf[5];

	struct iovec iov[1];
	iov[0].iov_base = buf;
	iov[0].iov_len = 5;

	struct msghdr msg;
	struct cmsghdr *cmptr;

	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;

	cmptr = malloc(CMSG_SPACE(2 * sizeof(int)));
	if (cmptr == NULL) {
		perror("malloc");
		exit(0);
	}

	msg.msg_control = (caddr_t) cmptr;
	msg.msg_controllen = CMSG_SPACE(2 * sizeof(int));

	r = recvmsg(virtual_map_socket, &msg, 0);
	if (r < 0) {
		perror("recvmsg");
		exit(0);
	}

	int* cmsg_data = (int*) CMSG_DATA(cmptr);
	ipv4_hv_socket = cmsg_data[0];
	if (cmptr->cmsg_len == CMSG_LEN(2 * sizeof(int))) {
		ipv6_hv_socket = cmsg_data[1];
	}

	r = connect(virtual_map_socket, (struct sockaddr *) &remote_addr, sizeof(struct sockaddr_un));
	if (r < 0) {
		perror("connect VMS");
		exit(0);
	}
}

ssize_t sendtov(int real_socket, const void* message, size_t length, int flags, const struct sockaddr* dest_addr, socklen_t dest_len) {
	if (_virtual && (real_socket == skfd || real_socket == skfd6)) {
		if (real_socket == skfd) {
			return sendto(ipv4_hv_socket, message, length, flags, dest_addr, dest_len);
		} else {
			return sendto(ipv6_hv_socket, message, length, flags, dest_addr, dest_len);
		}
	} else {
		return sendto(real_socket, message, length, flags, dest_addr, dest_len);
	}
}

