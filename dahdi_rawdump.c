/*
 * Capturing a raw dump from the DAHDI interface
 * 
 * Copyright (C) 2011 Torrey Searle
 * Copyright (C) 2022 Harald Welte
 * 
 */

/*
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2 as published by the
 * Free Software Foundation. See the LICENSE file included with
 * this program for more details.
 */

#include <stdio.h>
#include <fcntl.h>
#include <dahdi/user.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>

#define BLOCK_SIZE 512
#define MAX_CHAN 16

struct chan_fds {
	int rfd;
	int tfd;
	int chan_id;
	int rx_file;
	int tx_file;
};

static int make_mirror(long type, int chan)
{
	int res = 0;
	int fd = 0;	
	struct dahdi_bufferinfo bi;
	fd = open("/dev/dahdi/pseudo", O_RDONLY);

	memset(&bi, 0, sizeof(bi));
        bi.txbufpolicy = DAHDI_POLICY_IMMEDIATE;
        bi.rxbufpolicy = DAHDI_POLICY_IMMEDIATE;
        bi.numbufs = 32;
        bi.bufsize = BLOCK_SIZE;

	ioctl(fd, DAHDI_SET_BUFINFO, &bi);

	res = ioctl(fd, type, &chan);

	if(res)
	{
		printf("error setting channel err=%d!\n", res);
		return -1;
	}

	
	return fd;
}

static void log_packet(struct chan_fds * fd, char is_read)
{
	unsigned char buf[BLOCK_SIZE * 4];
	int res = 0;

	memset(buf, 0, sizeof(buf));

	if (is_read) {
		res = read(fd->rfd, buf, sizeof(buf));
		if (res <= 0)
			exit(23);
		write(fd->rx_file, buf, res);
	} else {
		res = read(fd->tfd, buf, sizeof(buf));
		if (res <= 0)
			exit(23);
		write(fd->tx_file, buf, res);
	}
}

static void usage(void)
{
	printf("Usage: dahdi_pcap [OPTIONS]\n");
	printf("Capture packets from DAHDI channels to pcap file\n\n");
	printf("Options:\n");
	printf("  -c, --chan=<channels>     Comma separated list of channels to capture from, max %d. Mandatory\n", MAX_CHAN);
	printf("  -r, --role=[network|user] Is the local side the network or user side in ISDN?\n");
	printf("  -f, --file=<filename>     The pcap file to capture to. Mandatory\n");
	printf("  -h, --help                Display this text\n");
}

int main(int argc, char **argv)
{
	struct chan_fds chans[MAX_CHAN];
	char *filename = NULL;
	int num_chans = 0;
	int max_fd = 0;

	int i;
	int c;

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{"chan", required_argument, 0, 'c'},
			{"file", required_argument, 0, 'f'},
			{"help", 0, 0, 'h'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "c:f:?", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
			case 'c':
				// Channels, comma separated list
				while(optarg != NULL && num_chans < MAX_CHAN) {
					int chan = atoi(strsep(&optarg, ","));

					chans[num_chans].tfd = make_mirror(DAHDI_TXMIRROR, chan);
					chans[num_chans].rfd = make_mirror(DAHDI_RXMIRROR, chan);
					chans[num_chans].chan_id = chan;

					if (chans[num_chans].tfd > max_fd)
						max_fd = chans[num_chans].tfd;

					if (chans[num_chans].rfd > max_fd)
						max_fd = chans[num_chans].rfd;

					num_chans++;
				}
				max_fd++;
				break;
			case 'f':
				filename = optarg;
				break;
			case 'h':
			default:
				// Usage
				usage();
				exit(0);
		  }
	}
	if ((num_chans == 0) || (filename == NULL)) {
		usage();
		exit(0);
	} else {
		printf("Capturing on channels ");
		for (i = 0; i < num_chans; i++) {
			printf("%d", chans[i].chan_id);
			if (i<num_chans-1)
				printf(", ");
		}
		printf(" to file [basename] %s\n", filename);
	}

	for (i = 0; i < num_chans; i++) {
		char fnamebuf[32+strlen(filename)];

		snprintf(fnamebuf, sizeof(fnamebuf), "%s-%u-rx.raw", filename, chans[i].chan_id);
		chans[i].rx_file = open(fnamebuf, O_CREAT | O_TRUNC | O_WRONLY);
		if (chans[i].rx_file < 0) {
			fprintf(stderr, "Cannot open %s: %s\n", fnamebuf, strerror(errno));
			exit(1);
		}

		snprintf(fnamebuf, sizeof(fnamebuf), "%s-%u-tx.raw", filename, chans[i].chan_id);
		chans[i].tx_file = open(fnamebuf, O_CREAT | O_TRUNC | O_WRONLY);
		if (chans[i].tx_file < 0) {
			fprintf(stderr, "Cannot open %s: %s\n", fnamebuf, strerror(errno));
			exit(1);
		}

	}

	while (1) {
		fd_set rd_set;
		FD_ZERO(&rd_set);
		for(i = 0; i < num_chans; i++) {
			FD_SET(chans[i].tfd, &rd_set);
			FD_SET(chans[i].rfd, &rd_set);
		}

		select(max_fd, &rd_set, NULL, NULL, NULL);

		for (i = 0; i < num_chans; i++) {
			if (FD_ISSET(chans[i].rfd, &rd_set))
				log_packet(&chans[i], 1);

			if (FD_ISSET(chans[i].tfd, &rd_set))
				log_packet(&chans[i], 0);
		}
		fputc('.', stdout);
		fflush(stdout);
	}

	return 0;
}
