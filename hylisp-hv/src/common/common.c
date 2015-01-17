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

#include "common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int hv_debug = 0;

void check_allocation(void* ptr) {
	if (ptr == NULL) {
		fprintf(stderr, "Unable to allocate memory\n");
		exit(-1);
	}
}

void fatal(char* msg) {
	fputs(msg, stderr);
	fprintf(stderr, "\nerrno is %d\n", errno);
	perror(NULL);
	
	if (hv_debug) {
		fflush(stderr);
		abort();
	} else {
		exit(-1);
	}
}

void fatalr(char* msg, int custom_errno) {
	fputs(msg, stderr);
	fprintf(stderr, "\nerrno is %d\n", custom_errno);
	perror(NULL);
	
	if (hv_debug) {
		fflush(stderr);
		abort();
	} else {
		exit(-1);
	}
}

void warning(char* msg) {
	fputs(msg, stderr);
	fprintf(stderr, "\nerrno is %d\n", errno);
	perror(NULL);
	fflush(stderr);
}

void warningr(char* msg, int custom_errno) {
	fputs(msg, stderr);
	fprintf(stderr, "\nerrno is %d\n", custom_errno);
	perror(NULL);
	fflush(stderr);
}

void debug_printf(const char* template, ...) {
	if (!hv_debug) {
		return;
	}

	char* nl_template = malloc(strlen(template) + 2);
	check_allocation(nl_template);
	strcpy(nl_template, template);
	strcat(nl_template, "\n");
	
	va_list args;
	va_start(args, template);
	vfprintf(stdout, nl_template, args);
	fflush(stdout);
	va_end(args);
	
	free(nl_template);
}

void debug_printf_prefix(ipv6_prefix* subject) {
	if (!hv_debug) {
		return;
	}
	
	char buf[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, subject->prefix, buf, INET6_ADDRSTRLEN);
	
	if (subject->reason == PRFX_ASIS) {
		fprintf(stdout, "Affected EID is %.*s/%u\n", INET6_ADDRSTRLEN, buf, subject->prefix_length);
	} else {
		fprintf(stdout, "Affected EID is %.*s/%u (reason %d)\n", INET6_ADDRSTRLEN, buf, subject->prefix_length, subject->reason);
	}
	fflush(stdout);
}