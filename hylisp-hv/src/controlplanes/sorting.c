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

#include "sorting.h"

#include "assignments.h"
#include "connected.h"
#include "string.h"

assignment** assignments;
int assignments_ctr;

static int precomputed_masks[8] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };

static int flex_sort(ipv6_prefix*, int*, int);

/* Requires control_planes rdlock and assignments rdlock */
int sort(ipv6_prefix* significant_eid, int* control_plane_index) {
	return flex_sort(significant_eid, control_plane_index, 0);
}

/* Requires control_planes rdlock and assignments rdlock */
int exact_sort(ipv6_prefix* significant_eid, int* control_plane_index) {
	return flex_sort(significant_eid, control_plane_index, 1);
}

static int flex_sort(ipv6_prefix* significant_eid, int* control_plane_index, int exact) {
	if (control_planes_ctr == 0) {
		return SORTING_ERR;
	}

	int j;

	for (j = 0; j < 16; j++) {
		if (significant_eid->prefix[j] != 0x00) {
			break;
		}
	}

	if (j == 16) {
		switch (significant_eid->prefix_length) {
		case 0:
			return SORTING_ALL;
		case 128:
			return SORTING_ERR;
		}
	}

	int i;
	int max_prefix_lenght = -1;

	for (i = 0; i < assignments_ctr; i++) {
		int match = 0;
		
		if (exact && significant_eid->prefix_length != assignments[i]->eid.prefix_length) {
			continue;
		}

		if (significant_eid->prefix_length >= assignments[i]->eid.prefix_length) {
			/* Sanitizing prefix_length */
			if (significant_eid->prefix_length > 128) {
				significant_eid->prefix_length = 128;
			}

			int whole_bytes = assignments[i]->eid.prefix_length / 8;
			int left_bits = assignments[i]->eid.prefix_length % 8;

			/* Checking whole bytes */
			if (memcmp(&(significant_eid->prefix), &(assignments[i]->eid.prefix), whole_bytes) == 0) {
				if (left_bits == 0) {
					match = 1;
				} else {
					/* Checking leftover bits */
					uint8_t challenge = significant_eid->prefix[whole_bytes] & precomputed_masks[left_bits];
					uint8_t stored = assignments[i]->eid.prefix[whole_bytes] & precomputed_masks[left_bits];

					if (challenge == stored) {
						match = 1;
					}
				}
			}
		}
		
		/* Prefix is longer (or equal) than previous entries */
		if (match && assignments[i]->eid.prefix_length >= max_prefix_lenght) {
			max_prefix_lenght = assignments[i]->eid.prefix_length;
			*(control_plane_index) = assignments[i]->assignee_index;
		}
	}

	if (max_prefix_lenght > -1) {
		return SORTING_ONE;
	} else {
		*(control_plane_index) = control_planes_def;
		return SORTING_NON;
	}
}

ipv6_prefix get_undefined_eid(int reason) {
	ipv6_prefix significant_eid = { { 0 }, 128, reason };
	return significant_eid;
}

ipv6_prefix get_broadcast_eid(int reason) {
	ipv6_prefix significant_eid = { { 0 }, 0, reason };
	return significant_eid;
}
