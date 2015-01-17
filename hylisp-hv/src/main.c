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

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>

#include "common/common.h"
#include "controlpackets/demux.h"
#include "controlplanes/register.h"
#include "mapmessages/demux.h"
#include "mapmessages/mux.h"

int hv_debug;

int main(int argc, char **argv) {
	int r;
	int c;
	
	int daemon = 0;
	
	opterr = 0;
	while ((c = getopt(argc, argv, "do:O:e:E:")) != -1) {
		switch (c) {
		case 'd':
			daemon = 1;
			break;
		case 'o':
			hv_debug = 1;
			freopen(optarg, "w", stdout);
			break;
		case 'O':
			hv_debug = 1;
			freopen(optarg, "a", stdout);
			break;
		case 'e':
			freopen(optarg, "w", stderr);
			break;
		case 'E':
			freopen(optarg, "a", stderr);
			break;
		case '?':
			if (optopt == 'o') {
				hv_debug = 1;
			} else if (isprint(optopt)) {
				fprintf(stderr, "Unknown option -%c\n", optopt);
			}
			break;
		}
	}
	
	if (daemon) {
		pid_t pid;

		pid = fork();
		if (pid < 0) {
			exit(EXIT_FAILURE);
		}

		if (pid > 0) {
			exit(EXIT_SUCCESS);
		}

		FILE* fpid;
		pid = getpid();
		fpid = fopen("/var/run/hylisphv.pid", "w");
		fprintf(fpid, "%d", pid);
		fclose(fpid);
	}

	pthread_t rs_id;
	r = pthread_create(&rs_id, NULL, start_registering_server, NULL);
	if (r != 0) {
		fatalr("Unable to start eastbound listener", r);
	}

	pthread_t cps_id;
	r = pthread_create(&cps_id, NULL, start_controlpackets_demuxer, NULL);
	if (r != 0) {
		fatalr("Unable to start westbound listeners", r);
	}

	pthread_t mmm_id;
	r = pthread_create(&mmm_id, NULL, start_mapmessages_muxer, NULL);
	if (r != 0) {
		fatalr("Unable to start northbound loop", r);
	}

	pthread_t mmd_id;
	r = pthread_create(&mmd_id, NULL, start_mapmessages_demuxer, NULL);
	if (r != 0) {
		fatalr("Unable to start southbound listener", r);
	}

	pthread_join(rs_id, NULL);
	pthread_join(cps_id, NULL);
	pthread_join(mmd_id, NULL);
	pthread_join(mmm_id, NULL);

	return 0;
}
