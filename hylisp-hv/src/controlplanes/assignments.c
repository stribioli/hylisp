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

#include "assignments.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../common/common.h"
#include "sorting.h"

static void expand_assignments();

static const int DYN_ARRAY_INIT = 128;
static const double DYN_ARRAY_INCR = 2;

assignment** assignments = NULL;
int assignments_ctr = 0;
int assignments_max = 0;
pthread_rwlock_t assignments_lock;

void init_assignments() {
	assignments = malloc(DYN_ARRAY_INIT * sizeof(assignment*));
	check_allocation(assignments);

	assignments_ctr = 0;
	assignments_max = DYN_ARRAY_INIT;
	
	pthread_rwlock_init(&assignments_lock, NULL);
}

static void expand_assignments() {
	assignments_max = DYN_ARRAY_INCR * assignments_max;

	assignments = realloc(assignments, assignments_max * sizeof(assignment*));
	check_allocation(assignments);
}

/* Requires assignments wrlock */
int add_assignment(assignment* item) {
	/* Checking if already present */
	int present_index;
	int sorting = exact_sort(&(item->eid), &present_index);
	switch (sorting) {
		case SORTING_ONE:
			/* Overwriting in place */
			assignments[present_index]->assignee_index = item->assignee_index;
			return present_index;
		case SORTING_ERR:
			/* Invalid EID */
			return -1;
	}
	
	if (assignments_ctr > assignments_max) {
		fatal("Illegal state");
	}
	
	if (assignments_ctr == assignments_max) {
		expand_assignments();
	}

	int ind = assignments_ctr++;
	assignments[ind] = malloc(sizeof(assignment));
	check_allocation(assignments[ind]);
	
	memcpy(assignments[ind], item, sizeof(assignment));

	debug_printf("New assignment registered at index %d by control plane at index %d", ind, item->assignee_index);
	debug_printf_prefix(&(item->eid));
	
	return ind;
}

/* Requires assignments wrlock */
void remove_assignment(assignment* item) {
	int present_index;
	int sorting = exact_sort(&(item->eid), &present_index);
	
	if (sorting != SORTING_ONE) {
		debug_printf("Assignment not removed: not present (requested by control plane at index %d)", item->assignee_index);
		debug_printf_prefix(&(item->eid));
		return;
	}
	
	if (item->assignee_index != assignments[present_index]->assignee_index) {
		debug_printf("Assignment not removed: control plane at index %d made the request, but the assignee is at index %d", item->assignee_index, assignments[present_index]->assignee_index);
		debug_printf_prefix(&(item->eid));
		return;
	}
	
	free(assignments[present_index]);
	
	memmove(&(assignments[present_index]), &(assignments[present_index + 1]), (assignments_ctr - 1 - present_index) * sizeof(assignment*));
	
	assignments_ctr--;
	
	debug_printf("Assignment removed from index %d (requested by control plane at index %d)", present_index, item->assignee_index);
	debug_printf_prefix(&(item->eid));
}

/* Requires assignments wrlock */
int remove_assignee(int assignee_index) {
	int count = 0;
	
	int i = 0;
	while (i < assignments_ctr) {
		if (assignments[i]->assignee_index == assignee_index) {
			free(assignments[i]);
			memmove(&(assignments[i]), &(assignments[i + 1]), (assignments_ctr - 1 - i) * sizeof(assignment*));
			assignments_ctr--;
		} else {
			i++;
		}
	}
	
	return count;
}
