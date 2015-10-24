/*-
 * Copyright (c) 2015 HGST, Inc.
 * All rights reserved.
 *
 * Copyright (c) 2013 EMC Corp.
 * All rights reserved.
 *
 * Copyright (C) 2012-2013 Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/ioccom.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nvmecontrol.h"

static void
pwrstate_usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, PWRSTATE_USAGE);
	exit(1);
}

void
pwrstate(int argc, char *argv[])
{
	int				i;
	int				fd = -1, state = 0;
	int				s_flag;
	char				ch, *p = NULL;
	char				*controller = NULL;
	struct nvme_controller_data	cdata;

	s_flag = false;

	while ((ch = getopt(argc, argv, "s:")) != -1) {
		switch (ch) {
		case 's':
			state = strtol(optarg, &p, 0);
			if (p != NULL && *p != '\0') {
				fprintf(stderr,
				    "\"%s\" not a valid power state.\n",
				    optarg);
				pwrstate_usage();
			} else if (state == 0) {
				fprintf(stderr,
				    "0 is not a valid power state. "
				    "Power state numbers start at 1.\n");
				pwrstate_usage();
			} else if (state > 32) {
				fprintf(stderr,
				    "State number %s specified which is "
				    "greater than max allowed state number of "
				    "32.\n", optarg);
				pwrstate_usage();
			}
			s_flag = true;
			break;
		}
	}

	/* Check that a controller (and not a namespace) was specified. */
	if (optind >= argc || strstr(argv[optind], NVME_NS_PREFIX) != NULL)
		pwrstate_usage();

	controller = argv[optind];
	open_dev(controller, &fd, 1, 1);
	read_controller_data(fd, &cdata);

        if(s_flag) {
		/* set the power state with SET FEATURES command */
		write_power_state(fd, state-1);
	} else {
		int curlvl = read_power_state(fd);

		/* get current power state from features so we can output
                   what the current level is in the list */

        	printf("Supported Power States\n");
        	printf("===========================\n");
        	for(i=0; i<cdata.npss+1; ++i) { /* cdata.npss is "0 based" so we add 1 to it */
			printf("Power State %u = %u W", i+1, cdata.psd[i].max_power/100);
			if(cdata.psd[i].flags & NVME_PS_FLAGS_NON_OP_STATE) {
				printf(" [no I/O]");
			}
			if(i == curlvl) {
				printf(" (Active)\n");
			} else {
				printf("\n");
			}
        	}
	}

	close(fd);
	exit(0);
}
