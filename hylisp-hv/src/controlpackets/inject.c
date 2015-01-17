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

#include "inject.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include "../common/common.h"

static uint16_t checksum_ipv4(uint16_t*);
static uint16_t checksum_udp(uint16_t*, uint8_t*, int);
static uint16_t checksum(uint16_t*, int);

int inject_datagram_ipv4(ipv4_datagram* datagram) {
	int s;
	s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);

	if (s == -1) {
		warning("Unable to create raw injecting socket");
		return -1;
	}
	
	shutdown(s, SHUT_RD);
	
	const int one = 1;
	setsockopt(s, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one));

	uint16_t header[14];

	header[0] = htons(0x4500);								// Version, IHL, DSCP, ECN
	
	header[1] = 20 + 8 + datagram->payload_len;				// Total length (IP header + UDP header + payload)
															// FreeBSD requires a host order value
	
	header[2] = htons(0x0001);								// Identification
	header[3] = htons(0x0000);								// Flags, fragment offset
	header[4] = htons(0x1111);								// TTL, protocol

	header[5] = 0x0000;										// Checksum (dummy)
	
	/* Source IP address */
	memcpy(&(header[6]), &(datagram->source.sin_addr.s_addr), 4);

	/* Destination IP address, localhost */
	memcpy(&(header[8]), &(datagram->destination.sin_addr.s_addr), 4);
	
	header[5] = checksum_ipv4(header);
	
	/* UDP header */
	header[10] = datagram->source.sin_port;					// Source port
	header[11] = htons(datagram->destination.sin_port);	// Destination port
	header[12] = htons(8 + datagram->payload_len);			// Length

	header[13] = 0x0000;									// Checksum (dummy)
	header[13] = checksum_udp(header, datagram->payload, datagram->payload_len);

	const int packet_len = 20 + 8 + datagram->payload_len;
	uint8_t* buf;
	buf = malloc(packet_len);
	check_allocation(buf);
	memcpy(buf, header, 20 + 8);
	memcpy(buf + 20 + 8, datagram->payload, datagram->payload_len);

	int r = sendto(s, buf, packet_len, 0, (struct sockaddr*) &(datagram->destination), sizeof(datagram->destination));

	if (r == -1) {
		warning("Unable to write to raw injecting socket");
		free(buf);
		close(s);
		return -1;
	}

	free(buf);
	close(s);

	return 0;
}

static uint16_t checksum_ipv4(uint16_t* header) {
	const int header_len = 20;
	return checksum(header, header_len);
}

static uint16_t checksum_udp(uint16_t* true_header, uint8_t* payload, int payload_len) {
	const int pseudo_header_len = 20;

	uint16_t pseudo_header[10];

	/* Source IP address */
	pseudo_header[0] = true_header[6];
	pseudo_header[1] = true_header[7];

	/* Destination IP address */
	pseudo_header[2] = true_header[8];
	pseudo_header[3] = true_header[9];

	pseudo_header[4] = htons(0x0011);				// Zeroes, protocol
	pseudo_header[5] = htons(8 + payload_len);		// UDP length

	/* UDP header */
	pseudo_header[6] = true_header[10];
	pseudo_header[7] = true_header[11];
	pseudo_header[8] = true_header[12];
	pseudo_header[9] = 0x0000;

	uint8_t* buf;

	buf = malloc(pseudo_header_len + payload_len);
	check_allocation(buf);

	memcpy(buf, pseudo_header, pseudo_header_len);
	memcpy(buf + pseudo_header_len, payload, payload_len);

	/* Zero-pad if odd number of octets */
	if (payload_len & 1) {
		buf = realloc(buf, pseudo_header_len + payload_len + 1);
		check_allocation(buf);

		*(buf + pseudo_header_len + payload_len) = 0;
		payload_len++;
	}

	uint16_t result;
	result = checksum((uint16_t*) buf, pseudo_header_len + payload_len);

	free(buf);

	return result;
}

static uint16_t checksum(uint16_t* subject, int subject_len) {
	unsigned long sum = 0;

	while (subject_len > 1) {
		sum += *subject++;

		if (sum & 0x80000000) {
			sum = (sum & 0xFFFF) + (sum >> 16);
		}

		subject_len -= 2;
	}

	while (sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}

	return (~sum);
}
