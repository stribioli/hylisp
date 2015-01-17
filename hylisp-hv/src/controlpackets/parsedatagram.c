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

#include "parsedatagram.h"

#include <arpa/inet.h>
#include <string.h>

#include "../common/common.h"

#define IRC_MAX 32

#define AFI_EMPTY	0
#define AFI_IPV4	1
#define AFI_IPV6	2

#define LISP_PKTTYPE_MAPREQUEST		1
#define LISP_PKTTYPE_MAPREPLY		2
#define LISP_PKTTYPE_MAPREGISTER	3
#define LISP_PKTTYPE_NOTIFY			4
#define LISP_PKTTYPE_ENCAPSULATED	8

static ipv6_prefix extract_eid_maprequest(uint8_t*, int);
static ipv6_prefix extract_eid_encapsulated(uint8_t*, int);
static void process_afi_address_couple(uint8_t*, ipv6_prefix*);
static int skip_afi_address_couple(uint8_t*, int*);

/*
 * Extracts significant EID from a control plane message.
 */
ipv6_prefix extract_eid_cp(uint8_t* datagram, int datagram_len) {
	uint8_t type;
	type = *datagram;
	type = type >> 4;

	debug_printf("Processing control packet of type 0x%x", type);

	switch (type) {
	case LISP_PKTTYPE_MAPREQUEST:
		return extract_eid_maprequest(datagram, datagram_len);
	case LISP_PKTTYPE_MAPREPLY:
		return get_broadcast_eid(BPRFX_MAPREPLY);
	case LISP_PKTTYPE_NOTIFY:
		return get_broadcast_eid(BPRFX_MAPNOTIFY); // TODO
	case LISP_PKTTYPE_ENCAPSULATED:
		return extract_eid_encapsulated(datagram, datagram_len);
	default:
		return get_undefined_eid(UPRFX_UNKNTYPE);
	}
}

/*
 * Extracts requested EID from a Map-Request message. Only the first record is read; currently, LISP specifications do not specify how to handle
 * multi-record messages.
 */
static ipv6_prefix extract_eid_maprequest(uint8_t* datagram, int datagram_len) {
	int r;

	int offset = 12; // Starting after nonce

	/* Source-EID field */
	r = skip_afi_address_couple(datagram, &offset);
	if (r < 0 || offset >= datagram_len) {
		return get_undefined_eid(UPRFX_MALFORMED);
	}

	/* ITR-RLOC fields */
	uint8_t rloc_count;
	// IRC is at offset 19-bit and is 5-bit long
	rloc_count = *(datagram + 2) & 0x1F;
	// RLOC count is IRC plus one
	rloc_count++;

	int i;
	for (i = 0; i < rloc_count; i++) {
		r = skip_afi_address_couple(datagram, &offset);
		if (r < 0 || offset >= datagram_len) {
			return get_undefined_eid(UPRFX_MALFORMED);
		}
	}

	ipv6_prefix significant_eid;
	significant_eid.reason = PRFX_ASIS;

	// Skip reserved bits
	offset++;
	
	/* EID Mask */
	significant_eid.prefix_length = *(datagram + offset);
	offset++;
	
	/* EID */
	process_afi_address_couple(datagram + offset, &significant_eid);

	return significant_eid;
}

static ipv6_prefix extract_eid_encapsulated(uint8_t* datagram, int datagram_len) {
	int offset = 4;							// Skip type and reserved

	uint8_t ip_version = (*(datagram + offset) & 0xF0) >> 4;	// IRC is at offset 0-bit and is 4-bit long

	int ihl;

	switch (ip_version) {
	case 4:
		ihl = (*(datagram + offset) & 0x0F) * 4;		// IHL is at offset 4-bit and is 4-bit long 
		offset = offset + ihl + 8;				// IPv4 header + UDP header
		break;
	case 6:
		offset = offset + 40 + 8;				// IPv6 header + UDP header
		break;
	default:
		return get_undefined_eid(UPRFX_UNKNIP);
	}

	if (offset >= datagram_len) {
		return get_undefined_eid(UPRFX_MALFORMED);
	}

	debug_printf("External header stripped (%d bytes long), processing inner packet.", offset);

	return extract_eid_cp(datagram + offset, datagram_len - offset);
}

static void process_afi_address_couple(uint8_t* afi_ptr, ipv6_prefix* significant_eid) {
	uint16_t afi = ntohs(*((uint16_t*) afi_ptr));

	uint8_t* eid_ptr = afi_ptr + 2;
	
	switch (afi) {
	case AFI_IPV4:
		/* IPv4, saving as IPv4-mapped IPv6 address */
		memset(significant_eid->prefix, 0, 16);
		significant_eid->prefix[10] = significant_eid->prefix[11] = 0xFF;
		memcpy(&(significant_eid->prefix[12]), eid_ptr, 4);

		significant_eid->prefix_length = significant_eid->prefix_length + 96;	// Fixing mask (from IPv4 to IPv6)
		break;
	case AFI_IPV6:
		/* IPv6, simply copying */
		memcpy(significant_eid->prefix, eid_ptr, 16);
		break;
	case AFI_EMPTY:
		*(significant_eid) = get_undefined_eid(UPRFX_EMPTYAFI);
		break;
	default:
		*(significant_eid) = get_undefined_eid(UPRFX_UNKNIP);
		break;
	}
}

static int skip_afi_address_couple(uint8_t* datagram, int* offset) {
	uint16_t afi = ntohs(*((uint16_t*) (datagram + *offset)));
	
	switch (afi) {
	case AFI_EMPTY:
		*offset = *offset + 2 + 0;
		break;
	case AFI_IPV4:
		*offset = *offset + 2 + 4;
		break;
	case AFI_IPV6:
		*offset = *offset + 2 + 16;
		break;
	default:
		return -1;
	}

	return 0;
}
