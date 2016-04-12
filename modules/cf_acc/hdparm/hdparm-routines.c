/* routines copied from hdparm.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/types.h>
#include "sgio.h"

#ifdef CONFIG_MAGICLANTERN
#include <machine/endian.h>
#include "hdparm-ml-shim.h"
#endif

const char *BuffType[4]		= {"unknown", "1Sect", "DualPort", "DualPortCache"};

int prefer_ata12 = 0;

static __u16 *id;
static void get_identify_data (int fd);

static void dump_sectors (__u16 *w, unsigned int count)
{
	unsigned int i;

	for (i = 0; i < (count*256/8); ++i) {
#if 0
		printf("%04x %04x %04x %04x %04x %04x %04x %04x\n",
			w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]);
		w += 8;
#else
		int word;
		for (word = 0; word < 8; ++word) {
			unsigned char *b = (unsigned char *)w++;
			printf("%02x%02x", b[0], b[1]);
			putchar(word == 7 ? '\n' : ' ');
		}
#endif
	}
}

static __u8 last_identify_op = 0;

static void get_identify_data (int fd)
{
	static __u8 args[4+512];
	int i;

	if (id)
		return;
	memset(args, 0, sizeof(args));
	last_identify_op = ATA_OP_IDENTIFY;
	args[0] = last_identify_op;
	args[3] = 1;	/* sector count */
	if (do_drive_cmd(fd, args, 0)) {
		prefer_ata12 = 0;
		memset(args, 0, sizeof(args));
		last_identify_op = ATA_OP_PIDENTIFY;
		args[0] = last_identify_op;
		args[3] = 1;	/* sector count */
		if (do_drive_cmd(fd, args, 0)) {
			perror(" HDIO_DRIVE_CMD(identify) failed");
			return;
		}
	}
	/* byte-swap the little-endian IDENTIFY data to match byte-order on host CPU */
	id = (void *)(args + 4);
	for (i = 0; i < 0x100; ++i) {
		unsigned char *b = (unsigned char *)&id[i];
		id[i] = b[0] | (b[1] << 8);	/* le16_to_cpu() */
	}
}

extern void identify (int fd, __u16 *id_supplied);

/* adapted from hdparm.c */
void hdparm_identify(int do_IDentity, int fd)
{
	if (do_IDentity) {
		get_identify_data(fd);
		if (id) {
			if (do_IDentity == 2)
				dump_sectors(id, 1);
			else
				identify(fd, (void *)id);
		}
	}
}
