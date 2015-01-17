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

#ifndef CONTROLPLANES_CONNECTED_H_
#define CONTROLPLANES_CONNECTED_H_

#include <pthread.h>
#include <stdint.h>

#include "assignments.h"

extern control_plane* control_planes;
extern int control_planes_ctr;
extern int control_planes_max;
extern int control_planes_def;
extern uint32_t control_planes_autoindex;
extern pthread_rwlock_t control_planes_lock;

void init_control_planes();
int add_control_plane(control_plane*);
control_plane_id remove_control_plane(int);

#endif /* CONTROLPLANES_CONNECTED_H_ */
