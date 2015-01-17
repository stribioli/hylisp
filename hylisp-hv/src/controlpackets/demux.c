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
#include <unistd.h>
#include <arpa/inet.h>

#include "../common/common.h"
#include "../controlplanes/connected.h"
#include "../controlplanes/sorting.h"
#include "inject.h"
#include "parsedatagram.h"

#define LISP_CONTROL_PORT "4342"
#define IP_MAXLEN 65535
#define SOCK_MSG_CONTROL_LEN 512

#ifdef LINUX_OS
#define IP_RECVDSTADDR 0
#endif

int ipv4_controlpackets_socket = -1;
int ipv6_controlpackets_socket = -1;

static void* start_controlpackets_demuxer_ipv4(void*);
static void process_ipv4_datagram(ipv4_datagram*);
static void send_ipv4_datagram_to_all(ipv4_datagram*);

void* start_controlpackets_demuxer(void* arg) {
	int r;

	pthread_attr_t tattr;
	pthread_attr_init(&tattr);
	pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

	pthread_t tid4;
	r = pthread_create(&tid4, &tattr, start_controlpackets_demuxer_ipv4, NULL);
	if (r != 0) {
		fatal("Unable to start westbound IPv4 listener thread");
	}

	// TODO IPv6

	pthread_exit(0);
}

static void* start_controlpackets_demuxer_ipv4(void* arg) {
	int r;

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	struct addrinfo* sorter_addr;
	r = getaddrinfo(NULL, LISP_CONTROL_PORT, &hints, &sorter_addr);
	if (r != 0) {
		fatalr("Unable to get westbound IPv4 listener address", r);
	}

	struct addrinfo* curr_addr;
	int s;
	for (curr_addr = sorter_addr; curr_addr != NULL; curr_addr = curr_addr->ai_next) {
		s = socket(curr_addr->ai_family, curr_addr->ai_socktype, curr_addr->ai_protocol);
		if (s == -1) {
			continue;
		}

		r = bind(s, curr_addr->ai_addr, curr_addr->ai_addrlen);
		if (r == -1) {
			continue;
		}

		freeaddrinfo(sorter_addr);
		break;
	}

	if (curr_addr == NULL) {
		fatal("Unable to bind westbound IPv4 listener");
	}

	ipv4_controlpackets_socket = s;

	debug_printf("IPv4 westbound server is listening");

	/*
	 * The datagram is kept in the stack space. Should the processing be offloaded to a pool of worker threads,
	 * it will be necessary to move it to the heap.
	 */
	uint8_t buf[IP_MAXLEN];
	char control_buf[SOCK_MSG_CONTROL_LEN];

	int opt = 1;
	setsockopt(s, IPPROTO_IP, IP_RECVDSTADDR, &opt, sizeof(opt));

	while (1) {
		ipv4_datagram datagram;
		datagram.payload = buf;

		struct msghdr raw_msg;
		struct iovec iov;

		raw_msg.msg_name = &(datagram.source);
		raw_msg.msg_namelen = sizeof(datagram.source);
		raw_msg.msg_iov = &iov;
		raw_msg.msg_iovlen = 1;
		raw_msg.msg_iov->iov_base = datagram.payload;
		raw_msg.msg_iov->iov_len = IP_MAXLEN;
		raw_msg.msg_control = (caddr_t) &control_buf;
		raw_msg.msg_controllen = SOCK_MSG_CONTROL_LEN;
		raw_msg.msg_flags = 0;

		datagram.payload_len = recvmsg(s, &raw_msg, 0);

		if (datagram.payload_len < 0) {
			fatal("Error reading from westbound IPv4 socket");
		}

		for (struct cmsghdr *c = CMSG_FIRSTHDR(&raw_msg); c != NULL; c = CMSG_NXTHDR(&raw_msg, c)) {
			if (c->cmsg_level != IPPROTO_IP || c->cmsg_type != IP_RECVDSTADDR) {
				continue;
			}

			struct in_addr* tmp_destination = (struct in_addr*) CMSG_DATA(c);

			memset(&(datagram.destination), 0, sizeof(datagram.destination));
#ifndef LINUX_OS
			datagram.destination.sin_len = sizeof(datagram.destination);
#endif
			datagram.destination.sin_family = AF_INET;
			datagram.destination.sin_addr = *tmp_destination;
		}

		char src[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(datagram.source.sin_addr), src, INET_ADDRSTRLEN);
		char dst[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(datagram.destination.sin_addr), dst, INET_ADDRSTRLEN);
		debug_printf("Processing UDPv4 datagram from %s to %s", src, dst);

		process_ipv4_datagram(&datagram);
	}

	pthread_exit(0);
}

static void process_ipv4_datagram(ipv4_datagram* datagram) {
	int r;

	uint16_t recipient_port = 0;

	ipv6_prefix significant_eid;
	significant_eid = extract_eid_cp(datagram->payload, datagram->payload_len);

	pthread_rwlock_rdlock(&control_planes_lock);
	pthread_rwlock_rdlock(&assignments_lock);

	int control_plane_index;
	r = sort(&significant_eid, &control_plane_index);

	if (r == SORTING_ONE || r == SORTING_NON) {
		recipient_port = control_planes[control_plane_index].port;
	}

	pthread_rwlock_unlock(&assignments_lock);
	pthread_rwlock_unlock(&control_planes_lock);

	switch (r) {
	case SORTING_ONE:
		debug_printf("Demuxing control datagram to port %d", recipient_port);
		debug_printf_prefix(&significant_eid);

		datagram->destination.sin_port = recipient_port;
		inject_datagram_ipv4(datagram);
		break;
	case SORTING_NON:
		debug_printf("Demuxing control datagram to port %d (default)", recipient_port);
		debug_printf_prefix(&significant_eid);

		datagram->destination.sin_port = recipient_port;
		inject_datagram_ipv4(datagram);
		break;
	case SORTING_ALL:
		debug_printf("Demuxing control datagram to all registered ports");
		debug_printf_prefix(&significant_eid);

		send_ipv4_datagram_to_all(datagram);
		break;
	case SORTING_ERR:
	default:
		debug_printf("Unable to demux control datagram");
		debug_printf_prefix(&significant_eid);
		break;;
	}
}

static void send_ipv4_datagram_to_all(ipv4_datagram* datagram) {
	pthread_rwlock_rdlock(&control_planes_lock);

	int i;
	for (i = 0; i < control_planes_ctr; i++) {
		datagram->destination.sin_port = control_planes[i].port;
		inject_datagram_ipv4(datagram);
	}

	pthread_rwlock_unlock(&control_planes_lock);
}
