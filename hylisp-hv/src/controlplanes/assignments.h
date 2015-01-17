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

#ifndef CONTROLPLANES_ASSIGNMENTS_H_
#define CONTROLPLANES_ASSIGNMENTS_H_

#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include "../common/common.h"

typedef uint32_t control_plane_id;

typedef struct {
	control_plane_id id;
	int socket_descriptor;
	uint16_t port;
	pid_t pid;
} control_plane;

typedef struct {
	ipv6_prefix eid;
	int assignee_index;
} assignment;

extern assignment** assignments;
extern int assignments_ctr;
extern int assignments_max;
extern pthread_rwlock_t assignments_lock;

void init_assignments();
int add_assignment(assignment*);
void remove_assignment(assignment*);
int remove_assignee(int);

#endif /* CONTROLPLANES_ASSIGNMENTS_H_ */
