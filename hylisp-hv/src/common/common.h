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

#ifndef COMMON_COMMON_H_
#define COMMON_COMMON_H_

#include <stdio.h>
#include <stdint.h>
#include <netinet/in.h>

#ifdef LINUX_OS
#define AF_MAP	38
#define PF_MAP	AF_MAP
#endif	/* LINUX_OS */

typedef struct {
	uint8_t prefix[16];
	uint8_t prefix_length;
	int reason;
} ipv6_prefix;

enum {
	PRFX_ASIS,				// The default value; if set, it means the extracted prefix already was an undefined or broadcast prefix
	UPRFX_EMPTYAFI,			// The record was correctly extracted, but it carried an empty value (signalled by a 0 AFI)
	UPRFX_NOEID,			// The message carried no useful record
	UPRFX_UNKNTYPE,			// The LISP Type value was unknown or unsupported
	UPRFX_UNKNVER,			// The socket used an unsupported message header version
	UPRFX_MALFORMED,		// The message was malformed
	UPRFX_UNKNIP,			// A packet or a record used an unknown IP version or AFI number
	BPRFX_MAPREPLY,			// The datagram was a Map Reply
	BPRFX_MAPNOTIFY			// The datagram was a Map Notify
};

typedef struct {
	struct sockaddr_in source;
	struct sockaddr_in destination;
	uint8_t* payload;
	int payload_len;
} ipv4_datagram;

extern int ipv4_controlpackets_socket;
extern int ipv6_controlpackets_socket;
extern int map_socket;

extern int hv_debug;

void check_allocation(void*);
void fatal(char*);
void fatalr(char*, int);
void warning(char*);
void warningr(char*, int);
void debug_printf(const char*, ...);
void debug_printf_prefix(ipv6_prefix*);

#endif /* COMMON_COMMON_H_ */
