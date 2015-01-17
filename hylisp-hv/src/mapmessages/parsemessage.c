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

#include "parsemessage.h"

#include <stdint.h>
#include <netinet/in.h>
#include <string.h>

#include "../common/common.h"

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
#define MAPM_ADD	   0x01
#define MAPM_DELETE	   0x02
#define MAPM_MISS_HEADER   0x07
#define MAPA_EID		0x01
#define MAPA_EIDMASK	0x02

/* Very brittle replacement of SA_SIZE from  FreeBSD <net/route.h> */
#define SA_SIZE(sa)						\
	((((struct sockaddr *)(sa))->sa_family == AF_INET) ? (16) : (32))

#else	/* LINUX_OS */

#include <sys/types.h>
#include <sys/socket.h>
#include <net/lisp/lisp.h>
#include <net/lisp/maptables.h>
#include <net/route.h>

#endif	/* LINUX_OS */

static ipv6_prefix extract_eid_mm_missheader(uint8_t*, int);
static ipv6_prefix extract_eid_mm_add_delete(uint8_t*, int);
static ipv6_prefix process_eid_ipv4(struct sockaddr_in*, int);
static ipv6_prefix process_eid_ipv6(struct sockaddr_in6*, int);

uint16_t extract_type_mm(uint8_t* buf, int message_length) {
	struct map_msghdr* header = (struct map_msghdr*) buf;

	return header->map_type;
}

uint32_t extract_flags_mm(uint8_t* buf, int message_length) {
	struct map_msghdr* header = (struct map_msghdr*) buf;

	return header->map_flags;
}

ipv6_prefix extract_eid_mm(uint8_t* buf, int message_length) {
	struct map_msghdr* header = (struct map_msghdr*) buf;
	if (header->map_versioning != 0x00) {
		return get_undefined_eid(UPRFX_UNKNVER);
	}

	switch (header->map_type) {
	case MAPM_ADD:
	case MAPM_DELETE:
		return extract_eid_mm_add_delete(buf, message_length);
	case MAPM_MISS_HEADER:
		return extract_eid_mm_missheader(buf, message_length);
	default:
		return get_undefined_eid(UPRFX_UNKNTYPE);
	}	
}

static ipv6_prefix extract_eid_mm_add_delete(uint8_t* buf, int message_length) {
	int offset = 0;

	struct map_msghdr* header = (struct map_msghdr*) (buf + offset);
	if (header->map_versioning != 0x00) {
		return get_undefined_eid(UPRFX_UNKNVER);
	}

	if (!(header->map_addrs & MAPA_EID)) {
		return get_undefined_eid(UPRFX_NOEID);
	}

	int has_mask = header->map_addrs & MAPA_EIDMASK;

	offset = offset + sizeof(struct map_msghdr);	// Skip header

	/* EID */
	struct sockaddr_storage* storage_eid;
	storage_eid = (struct sockaddr_storage*) (buf + offset);

	switch (storage_eid->ss_family) {
	case AF_INET:
		return process_eid_ipv4((struct sockaddr_in*) storage_eid, has_mask);
	case AF_INET6:
		return process_eid_ipv6((struct sockaddr_in6*) storage_eid, has_mask);
	default:
		return get_undefined_eid(UPRFX_UNKNIP);
	}
}

static ipv6_prefix process_eid_ipv4(struct sockaddr_in* raw_eid, int has_mask) {
	ipv6_prefix eid;
	eid.reason = PRFX_ASIS;
	
	/* Saving as IPv4-mapped IPv6 address */

	memset(eid.prefix, 0, 16);
	eid.prefix[10] = eid.prefix[11] = 0xFF;
	memcpy(&(eid.prefix[12]), &(raw_eid->sin_addr.s_addr), 4);
	
	if (has_mask) {
			uint8_t* ptr = (uint8_t*) raw_eid;
			ptr = ptr + SA_SIZE(raw_eid);

			struct sockaddr_in* raw_mask;
			raw_mask = (struct sockaddr_in*) ptr;

			ptr = (uint8_t*) &(raw_mask->sin_addr.s_addr);

			eid.prefix_length = 128;

			for (int i = 0; i < 4; i++) {
				if (*ptr == 0xFF) {
					ptr++;
				} else {
					eid.prefix_length = 96 + 8 * i;

					uint8_t bit_window = 0x80;
					for (int j = 0; j < 7; j++) {
						if (*ptr & bit_window) {
							eid.prefix_length++;
							bit_window = bit_window >> 1;
						} else {
							break;
						}
					}

					break;
				}
			}
		} else {
			eid.prefix_length = 128;
		}
	
	return eid;
}

static ipv6_prefix process_eid_ipv6(struct sockaddr_in6* raw_eid, int has_mask) {
	ipv6_prefix eid;
	eid.reason = PRFX_ASIS;

	memcpy(&(eid.prefix), &(raw_eid->sin6_addr.s6_addr), sizeof(struct in6_addr));

	if (has_mask) {
		uint8_t* ptr = (uint8_t*) raw_eid;
		ptr = ptr + SA_SIZE(raw_eid);

		struct sockaddr_in6* raw_mask;
		raw_mask = (struct sockaddr_in6*) ptr;

		ptr = (uint8_t*) &(raw_mask->sin6_addr.s6_addr);

		eid.prefix_length = 128;

		for (int i = 0; i < 16; i++) {
			if (*ptr == 0xFF) {
				ptr++;
			} else {
				eid.prefix_length = 8 * i;

				uint8_t bit_window = 0x80;
				for (int j = 0; j < 7; j++) {
					if (*ptr & bit_window) {
						eid.prefix_length++;
						bit_window = bit_window >> 1;
					} else {
						break;
					}
				}

				break;
			}
		}
	} else {
		eid.prefix_length = 128;
	}
	
	return eid;
}

static ipv6_prefix extract_eid_mm_missheader(uint8_t* buf, int message_length) {
	int offset = 0;

	struct map_msghdr* header = (struct map_msghdr*) buf;
	
	offset = sizeof(struct map_msghdr);	// Skip map_msghdr

	if (header->map_addrs & MAPA_EID) {
		offset = offset + SA_SIZE(buf + offset);
	}

	if (header->map_addrs & MAPA_EIDMASK) {
		offset = offset + SA_SIZE(buf + offset);
	}

	ipv6_prefix source_eid;
	source_eid.prefix_length = 128;
	source_eid.reason = PRFX_ASIS;

	uint8_t ip_ver;
	ip_ver = *(buf + offset) >> 4;

	switch (ip_ver) {
	case 0x4:
		offset = offset + 12;
		memset(source_eid.prefix, 0, 16);
		source_eid.prefix[10] = source_eid.prefix[11] = 0xFF;
		memcpy(&(source_eid.prefix[12]), buf + offset, 4);
		break;
	case 0x6:
		offset = offset + 8;
		memcpy(source_eid.prefix, buf + offset, 16);
		break;
	default:
		return get_undefined_eid(UPRFX_UNKNIP);
	}

	return source_eid;
}
