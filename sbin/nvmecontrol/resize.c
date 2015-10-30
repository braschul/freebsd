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

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nvmecontrol.h"

union nvme_vuc_dword12
{
    uint32_t raw;
    struct
    {
        uint8_t cmd;
        uint8_t subcmd;
        union {
            uint16_t rsvd;
            struct {
                uint16_t rsvd : 8;
                uint16_t clear : 8;
            } smart;
        };
    } fields;
} __packed;

void
send_resize(int fd, uint32_t size)
{
	struct nvme_pt_command	pt;
	union nvme_vuc_dword12  cdw12;

	memset(&cdw12, 0, sizeof(cdw12));
	cdw12.fields.cmd = 0x03;
	cdw12.fields.subcmd = 0x01;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = 0xCC;				/* HGST VUC */
	pt.cmd.cdw10 = 0;
	pt.cmd.cdw12 = cdw12.raw;
	pt.cmd.cdw13 = size;
	pt.is_read = 0;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(1, "resize request failed");

	if (nvme_completion_is_error(&pt.cpl))
		errx(1, "resize request returned error");

	/* resize command requires controller reset */
	if (ioctl(fd, NVME_RESET_CONTROLLER) < 0)
		err(1, "controller reset request failed");
	printf("Resize operation complete.  Please reboot.\n");
}

static void
resize_usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, RESIZE_USAGE);
	exit(1);
}

void
resize(int argc, char *argv[])
{
	int				fd = -1;
	uint32_t			size = 0;
	int				s_flag = false;
	char				ch, *p = NULL;
	char                            *controller = NULL;
	struct nvme_controller_data	cdata;

	while ((ch = getopt(argc, argv, "s:")) != -1) {
		switch (ch) {
		case 's':
			size = strtoul(optarg, &p, 0);
			if (p != NULL && *p != '\0') {
				fprintf(stderr,
				    "\"%s\" not a valid size.\n",
				    optarg);
				resize_usage();
			} else if (size == 0) {
				fprintf(stderr,
				    "Size must be larger than 0.\n");
				resize_usage();
			}
			s_flag = true;
			break;
		}
	}

	/* Check that a controller (and not a namespace) was specified. */
	if (optind >= argc || strstr(argv[optind], NVME_NS_PREFIX) != NULL)
		resize_usage();

	controller = argv[optind];
	open_dev(controller, &fd, 1, 1);
	read_controller_data(fd, &cdata);

	if(!s_flag) {
		printf("Missing size (-s).\n");
		resize_usage();
	}

	send_resize(fd, size);

	close(fd);
	exit(0);
}

