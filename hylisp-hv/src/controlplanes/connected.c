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

#include "connected.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "../common/common.h"

static const int DYN_ARRAY_INIT = 16;
static const double DYN_ARRAY_INCR = 2;

control_plane* control_planes = NULL;
int control_planes_ctr = 0;
int control_planes_max = 0;
int control_planes_def = 0;
uint32_t control_planes_autoindex = 0;
pthread_rwlock_t control_planes_lock;

static void expand_control_planes();

void init_control_planes() {
	control_planes = malloc(DYN_ARRAY_INIT * sizeof(control_plane));
	check_allocation(control_planes);
	
	control_planes_max = DYN_ARRAY_INIT;

	pthread_rwlock_init(&control_planes_lock, NULL);
}

static void expand_control_planes() {
	control_planes_max = DYN_ARRAY_INCR * control_planes_max;
	
	control_planes = realloc(control_planes, control_planes_max * sizeof(control_plane));
	check_allocation(control_planes);
}

/* Requires control_planes wrlock */
int add_control_plane(control_plane* cp) {
	if (control_planes_ctr > control_planes_max) {
		fatal("Illegal state");
	}
	
	if (control_planes_ctr == control_planes_max) {
		expand_control_planes();
	}

	int ind = control_planes_ctr++;
	memcpy(&(control_planes[ind]), cp, sizeof(control_plane));

	debug_printf("New control plane registered with id %d", control_planes_autoindex - 1);
	
	return ind;
}

/* Requires control_planes wrlock */
control_plane_id remove_control_plane(int ind) {
	control_plane_id id = control_planes[ind].id;

	memmove(&(control_planes[ind]), &(control_planes[ind + 1]), (control_planes_ctr - 1 - ind) * sizeof(control_plane));

	control_planes_ctr--;
	
	return id;
}
