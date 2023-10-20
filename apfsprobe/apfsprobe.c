/*
 * Copyright (C) 2022 Ernesto A. Fernández <ernesto@corellium.com>
 * Copyright (C) 2023 John Othwolo 
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <apfs/raw.h>

static char *progname;

/**
 * usage - Print usage information and exit
 */
static void usage(void)
{
	fprintf(stderr, "usage: %s mountpoint name\n", progname);
	exit(1);
}

/**
 * version - Print version information and exit
 */
static void version(void)
{
	printf("apfsprobe version 0.1\n");
	exit(1);
}

/**
 * system_error - Print a system error message and exit
 */
static __attribute__((noreturn)) void system_error(void)
{
	perror(progname);
	exit(1);
}

/**
 * fatal - Print a message and exit with an error code
 * @message: text to print
 */
static __attribute__((noreturn)) void fatal(const char *message)
{
	fprintf(stderr, "%s: %s\n", progname, message);
	exit(1);
}


static void 
read_super(char *device, int fd, struct apfs_nx_superblock *sb)
{
    int ret = pread(fd, sb, sizeof(struct apfs_nx_superblock), 0);
    if (ret < 0) {
        fprintf(stderr, "Failed to read superblock");
        system_error();
    }

	assert(sb->s_blocksize);

    if (le32_to_cpu(current->nx_magic) != APFS_NX_MAGIC)
        fatal("Not a superblock"); /* Not a superblock */
    if (!obj_verify_csum(&current->nx_o))
        fatal("Superblock is corrupted"); /* Corrupted */
}

static void 
print_volume_info(char *device, struct apfs_superblock *vcb, int index) 
{
    printf("    : %19s %llu\n", vcb->apfs_volname, vcb->apfs_fs_alloc_count);
}

static void 
print_volumes(char *device, struct apfs_nx_superblock *sb) 
{
    if (sb->nx_max_file_systems >= APFS_NX_MAX_FILE_SYSTEMS)
        fatal("Number of filesystems in container exceed limit");
    
    struct apfs_superblock *vcb[nx_max_file_systems] = {NULL};
	struct object objs[nx_max_file_systems] = {0};
    int vol_id;

    printf("/dev/%s:\n", device);
    printf("   #: NAME                    SIZE\n");

    for (int i = 0; i < sb->nx_max_file_systems; i++) 
    { 
        vol_id = le64_to_cpu(sb->nx_fs_oid[i]);
        if (vol_id == 0) {
            apfs_err(sb, "requested volume does not exist");
            return -EINVAL;
        }

        // read volume superblock
        vcb[i] = read_object(vol_id, sb->s_omap_table, &objs[i]);

        // print volume info
        print_volume_info(device, &vcb[i], i);
    }
}

int main(int argc, char *argv[])
{
    struct apfs_nx_superblock sb;
	const char *devicepath = NULL;
	int fd;

	if (argc == 0)
		exit(1);
	progname = argv[0];

	while (1) {
		int opt = getopt(argc, argv, "v");

		if (opt == -1)
			break;

		switch (opt) {
		case 'v':
			version();
		default:
			usage();
		}
	}

	if (optind != argc - 2)
		usage();
    
	devicepath = argv[optind];

	fd = open(devicepath, O_RDONLY);
	if (fd == -1)
		system_error();

    read_super(devicepath, fd, &sb);
    print_volumes(devicepath, &sb);

	return 0;
}
