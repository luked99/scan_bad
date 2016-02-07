/*
 *  nanddump.c
 *
 *  Copyright (C) 2000 David Woodhouse (dwmw2@infradead.org)
 *                     Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This utility dumps the contents of raw NAND chips or NAND
 *   chips contained in DoC devices.
 */

#define _GNU_SOURCE
#include <byteswap.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <asm/types.h>
#include <mtd/mtd-user.h>

#define PROGRAM "nanddump"
#define VERSION "$Revision: 1.29 $"

static void display_help (void)
{
	printf(
"Usage: scanbad [OPTIONS] MTD-device\n"
"Scan for bad blocks.\n"
"\n"
	);
	exit(EXIT_SUCCESS);
}

// Option variables

static const char	*mtddev;		// mtd device name

static unsigned char read_marker(mtd_info_t *meminfo, int fd, int offset)
{
	unsigned char oobbuf[128];
	struct mtd_oob_buf oob;
    oob.start = offset;
    oob.length = 16;
    oob.ptr = oobbuf;

	oob.length = meminfo->oobsize;
	memset(oobbuf, 0x42, sizeof(oobbuf));

	int rc = ioctl(fd, MEMREADOOB, &oob);
	if (rc < 0) {
		perror("MEMREADOOB");
		exit(1);
	}
	return oobbuf[0];
}

static void report_failure(int eraseblock, int page, unsigned oob, int *count)
{
    (*count)++;
	printf("eraseblock %d page %d oob[0] = 0x%x\n",
		eraseblock, page, oob);
}

int main(int argc, char * const argv[])
{
	int fd;
	mtd_info_t meminfo;

    if ( argc != 2 )
        display_help();

    mtddev = argv[1];

	/* Open MTD device */
	if ((fd = open(mtddev, O_RDONLY)) == -1) {
		perror(mtddev);
		exit (EXIT_FAILURE);
	}

    printf("scanning %s\n", mtddev);

	/* Fill in MTD device capability structure */
	if (ioctl(fd, MEMGETINFO, &meminfo) != 0) {
		perror("MEMGETINFO");
		close(fd);
		exit (EXIT_FAILURE);
	}

	/* Make sure device page sizes are valid */
	if (!(meminfo.oobsize == 128 && meminfo.writesize == 4096) &&
			!(meminfo.oobsize == 64 && meminfo.writesize == 2048) &&
			!(meminfo.oobsize == 32 && meminfo.writesize == 1024) &&
			!(meminfo.oobsize == 16 && meminfo.writesize == 512) &&
			!(meminfo.oobsize == 8 && meminfo.writesize == 256)) {
		fprintf(stderr, "Unknown flash (not normal NAND)\n");
		close(fd);
		exit(EXIT_FAILURE);
	}

	int pagesize = meminfo.writesize;
	int last_page = meminfo.erasesize / pagesize - 1;
	int total_erase_blocks = meminfo.size / meminfo.erasesize;
	int eraseblock;

    printf("%d erase blocks of %d pages size %d bytes\n",
            total_erase_blocks, meminfo.erasesize / pagesize,
            pagesize);

    int count = 0;
    int pages_checked = 0;

	for (eraseblock = 0; eraseblock < total_erase_blocks; eraseblock++)
	{
		unsigned offset = eraseblock * meminfo.erasesize;
		unsigned char byte0 = read_marker(&meminfo, fd, offset + 0 * pagesize);
		unsigned char byte1 = read_marker(&meminfo, fd, offset + 1 * pagesize);
		unsigned char byte2 = read_marker(&meminfo, fd, offset + last_page * pagesize);

        pages_checked += 3;
		if (byte0 != 0xff)
			report_failure(eraseblock, 0, byte0, &count);
		if (byte1 != 0xff)
			report_failure(eraseblock, 1, byte1, &count);
		if (byte2 != 0xff)
			report_failure(eraseblock, last_page, byte2, &count);
	}
    printf("%d bad blocks\n", count);
    printf("checked %d pages\n", pages_checked);
	close(fd);
	return 0;
}
