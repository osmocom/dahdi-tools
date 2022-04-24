/*
 * trunkdev configuration program for DAHDI Telephony Interface
 *
 * Copyright (C) 2022 Harald Welte <laforge@osmocom.org>
 * Copyright (C) 2001-2008 Digium, Inc.
 *
 * All rights reserved.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <sys/socket.h>
#include <dahdi/user.h>

#include "dahdi-sysfs.h"

static int g_fd = -1;


/* wrapper around DAHDI_TRUNKDEV_CREATE */
static int trunkdev_create(int fd, const char *name)
{
	struct dahdi_trunkdev_create td_c = { 0 };
	int rc;

	strncpy(td_c.name, name, sizeof(td_c.name));
	td_c.name[sizeof(td_c.name)-1] = 0;

	rc = ioctl(fd, DAHDI_TRUNKDEV_CREATE, &td_c);
	if (rc)
		fprintf(stderr, "Error creating trunkdev '%s': %s\n", name, strerror(errno));

	return rc;
}

/* wrapper around DAHDI_TRUNKDEV_DELETE */
static int trunkdev_delete(int fd, const char *name)
{
	struct dahdi_trunkdev_delete td_d = { 0 };
	int rc;

	strncpy(td_d.name, name, sizeof(td_d.name));
	td_d.name[sizeof(td_d.name)-1] = 0;

	rc = ioctl(fd, DAHDI_TRUNKDEV_DELETE, &td_d);
	if (rc)
		fprintf(stderr, "Error deleting trunkdev '%s': %s\n", name, strerror(errno));

	return rc;
}

/* wrapper around DAHDI_TRUNKDEV_SPECIFY */
static int trunkdev_specify(int fd, const char *name)
{
	struct dahdi_trunkdev_open td_o = { 0 };
	int rc;

	strncpy(td_o.name, name, sizeof(td_o.name));
	td_o.name[sizeof(td_o.name)-1] = 0;

	rc = ioctl(fd, DAHDI_TRUNKDEV_OPEN, &td_o);
	if (rc < 0) {
		fprintf(stderr, "Error opening trunkdev '%s': %s\n", name, strerror(errno));
		return rc;
	}
	return td_o.spanno;
}

/* wrapper around DAHDI_SPANCONFIG (assuming E1 with CRC4) */
static int spanconfig(int fd, int spanno)
{
	struct dahdi_lineconfig dlc = {
		.span = spanno,
		.lbo = 0,
		.lineconfig = DAHDI_CONFIG_CRC4,
		.sync = 0,
	};

	return ioctl(fd, DAHDI_SPANCONFIG, &dlc);
}

/* wrapper around DAHDI_CHANCONFIG (assuming E1/ALAW/CLEAR; unless dchan -> HDLCFCS) */
static int chanconfig(int fd, int channo, bool dchan)
{
	struct dahdi_chanconfig dcc = {
		.chan = channo,
		.sigtype = DAHDI_SIG_CLEAR,
		.deflaw = DAHDI_LAW_ALAW,
		.master = 0,
		.idlebits = 0,
	};
	if (dchan)
		dcc.sigtype = DAHDI_SIG_HDLCFCS;

	return ioctl(fd, DAHDI_CHANCONFIG, &dcc);
}

static int chanconfig_hdlc_master(int fd, int channo, const char *netdev_name)
{
	struct dahdi_chanconfig dcc = {
		.chan = channo,
		.sigtype = DAHDI_SIG_HDLCNET,
		.deflaw = DAHDI_LAW_ALAW,
		.master = 0,
		.idlebits = 0,
	};
	strncpy(dcc.netdev_name, netdev_name, sizeof(dcc.netdev_name));
	dcc.netdev_name[sizeof(dcc.netdev_name)-1] = '\0';

	return ioctl(fd, DAHDI_CHANCONFIG, &dcc);
}

static int chanconfig_hdlc_slave(int fd, int channo, int master)
{
	struct dahdi_chanconfig dcc = {
		.chan = channo,
		.sigtype = DAHDI_SIG_SLAVE,
		.deflaw = DAHDI_LAW_ALAW,
		.master = master,
		.idlebits = 0,
	};

	return ioctl(fd, DAHDI_CHANCONFIG, &dcc);
}

/* high-level wrapper to configure an E1 span + all its channels */
static int config(int spanno, const char *hdlc_netdev)
{
	struct dahdi_spaninfo dsi = {
		.spanno = spanno,
	};
	int fd, rc;

	fd = open("/dev/dahdi/ctl", O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Unable to open /dev/dahd/ctl: %s\n", strerror(errno));
		return fd;
	}

	rc = ioctl(fd, DAHDI_SPANSTAT, &dsi);
	if (rc < 0) {
		fprintf(stderr, "Error during spanstat: %s\n", strerror(errno));
		close(fd);
		return rc;
	}
	printf("Span %u (%s: %s)\n", dsi.spanno, dsi.name, dsi.desc);
	printf("  numchans=%u, totalchans=%u\n", dsi.numchans, dsi.totalchans);

	rc = spanconfig(fd, spanno);
	if (rc < 0) {
		fprintf(stderr, "Error during spanconfig: %s\n", strerror(errno));
		close(fd);
		return rc;
	}

	/* resolve basechan + num_chan via sysfs */
	int basechan = dahdi_span_get_basechan(spanno);
	int num_chan = dahdi_span_get_channels(spanno);
	if (basechan < 0 || num_chan < 0) {
		fprintf(stderr, "Error getting basechan=%d or channels=%d for spanno=%d: %s\n",
			basechan, num_chan, spanno, strerror(errno));
		close(fd);
		return -1;
	}

	for (unsigned int i = basechan; i < basechan+num_chan; i++) {
		if (!hdlc_netdev) {
			/* ISDN: we assume a standard ISDN configuration with TS16 == D-channel */
			rc = chanconfig(fd, i, i == basechan + 15);
		} else {
			/* HDLC: we assume TS 1-31 as superchannel */
			if (i == basechan)
				rc = chanconfig_hdlc_master(fd, i, hdlc_netdev);
			else
				rc = chanconfig_hdlc_slave(fd, i, basechan);
		}
		if (rc < 0) {
			fprintf(stderr, "Error during spanconfig: %s\n", strerror(errno));
			close(fd);
			return rc;
		}
	}
	close(fd);
	return 0;
}

static void print_help(void)
{
	printf("\n");
	printf(	"dahdi_trunkdev create NAME\n"
		"   Create a new dahdi trunkdev named NAME\n"
		"dahdi_trunkdev delete NAME\n"
		"   Delete an existing dahdi trunkdev named NAME\n"
		"dahdi_trunkdev config NAME\n"
		"   Configure span + all channels of trunkdev named NAME for ISDN\n"
		"dahdi_trunkdev config-hdlc NAME\n"
		"   Configure span + all channels of trunkdev named NAME for HDLC\n"
		"dahdi_trunkdev read NAME\n"
		"   Continuously read + discard frames from trunkdev named NAME\n"
		"dahdi_trunkdev rdwr NAME\n"
		"   Continuously read + loop back (write) frames of trunkdev named NAME\n");

}

int main(int argc, char **argv)
{
	int rc;

	printf("DAHDI trunkdev tool (C) 2022 by Harald Welte <laforge@osmocom.org>\n\n");

	if (argc < 2) {
		fprintf(stderr, "First argument must specify the command\n");
		print_help();
		exit(2);
	}

	if (!strcmp(argv[1], "help") || !strcmp(argv[1], "--help")) {
		print_help();
		exit(0);
	}

	g_fd = open("/dev/dahdi/trunkdev", O_RDWR);
	if (g_fd < 0) {
		fprintf(stderr, "Unable to open trunkdev: %s\n", strerror(errno));
		exit(1);
	}

	if (!strcmp(argv[1], "create")) {
		/* dahdi_trunkdev create foo */
		if (argc < 3) {
			fprintf(stderr, "Second argument must specify the name\n");
			exit(2);
		}
		rc = trunkdev_create(g_fd, argv[2]);
		if (rc < 0) {
			fprintf(stderr, "Unable to create trunkdev: %s\n", strerror(errno));
			exit(1);
		}
		printf("DAHDI trunkdev %s created; span number %d\n", argv[2], rc);
	} else if (!strcmp(argv[1], "delete")) {
		/* dahdi_trunkdev create foo */
		if (argc < 3) {
			fprintf(stderr, "Second argument must specify the name\n");
			exit(2);
		}
		rc = trunkdev_delete(g_fd, argv[2]);
		if (rc < 0) {
			fprintf(stderr, "Unable to create trunkdev: %s\n", strerror(errno));
			exit(1);
		}
		printf("DAHDI trunkdev %s deleted\n", argv[2]);
	} else if (!strcmp(argv[1], "config")) {
		/* dahdi_trunkdev config foo */
		if (argc < 3) {
			fprintf(stderr, "Second argument must specify the name\n");
			exit(2);
		}
		rc = trunkdev_specify(g_fd, argv[2]);
		if (rc < 0) {
			fprintf(stderr, "Unable to specify trunkdev: %s\n", strerror(errno));
			exit(2);
		}
		rc = config(rc, NULL);
		if (rc < 0)
			exit(2);
		printf("DAHDI trunkdev %s configured\n", argv[2]);
	} else if (!strcmp(argv[1], "config-hdlc")) {
		/* dahdi_trunkdev config-hdlc foo hdlc-foo */
		if (argc < 3) {
			fprintf(stderr, "Second argument must specify the name\n");
			exit(2);
		}
		if (argc < 4) {
			fprintf(stderr, "Third argument must specify the net-device name\n");
			exit(2);
		}
		rc = trunkdev_specify(g_fd, argv[2]);
		if (rc < 0) {
			fprintf(stderr, "Unable to specify trunkdev: %s\n", strerror(errno));
			exit(2);
		}
		rc = config(rc, argv[3]);
		if (rc < 0)
			exit(2);
		printf("DAHDI trunkdev %s configured as netdev %s\n", argv[2], argv[3]);
	} else if (!strcmp(argv[1], "read") || !strcmp(argv[1], "rdwr")) {
		/* dahdi_trunkdev (read|rdwr) foo */
		if (argc < 3) {
			fprintf(stderr, "Second argument must specify the name\n");
			exit(2);
		}
		rc = trunkdev_specify(g_fd, argv[2]);
		if (rc < 0) {
			fprintf(stderr, "Unable to specify trunkdev: %s\n", strerror(errno));
			exit(1);
		}
		if (!strcmp(argv[1], "read"))
			printf("Reading + Disccarding from DAHDI trunkdev %s\n", argv[2]);
		else
			printf("Reading + Writing (looping back) DAHDI trunkdev %s\n", argv[2]);
		while (1) {
			uint8_t buf[32*8];
			rc = read(g_fd, buf, sizeof(buf));
			if (rc <= 0) {
				fprintf(stderr, "Error %d reading: %s\n", rc, strerror(errno));
				exit(1);
			}
			fputc('.', stdout);
			if (!strcmp(argv[1], "rdwr")) {
				rc = write(g_fd, buf, rc);
				if (rc <= 0) {
					fprintf(stderr, "Error %d writing: %s\n", rc, strerror(errno));
					exit(1);
				}
			}
		}
	} else
		exit(23);

	exit(0);
}
