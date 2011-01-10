#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mount.h>

#include <aul/string.h>
#include <aul/error.h>
#include <kernel.h>
#include <kernel-priv.h>


#if !defined(INSTALL) || !defined(MEMFS)
#error "INSTALL and MEMFS directives must be defined"
#endif

#define MOUNT_PATH		(INSTALL "/" MEMFS)
#define MOUNT_NAME		"maxkernel_memfs"
#define MOUNT_TYPE		"ramfs"


void memfs_init(Error ** err)
{
	memfs_destroy(NULL);

	int success = mount(MOUNT_NAME, MOUNT_PATH, MOUNT_TYPE, MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL);
	if (success != 0 && err != NULL)
	{
		*err = error_new(errno, "Count not mount %s of type %s on point %s: %s", MOUNT_NAME, MOUNT_TYPE, MOUNT_PATH, strerror(errno));
	}
}

void memfs_destroy(Error ** err)
{
	int success = umount2(MOUNT_PATH, MNT_DETACH);
	if (success != 0 && err != NULL)
	{
		*err = error_new(errno, "Count not unmount %s on point %s: %s", MOUNT_NAME, MOUNT_PATH, strerror(errno));
	}
}

int memfs_orphanfd()
{
	String path = string_new("%s/orphan-%" PRIu64 "-%d", MOUNT_PATH, kernel_timestamp(), rand());
	int fd = open(path.string, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	unlink(path.string);

	return fd;
}

int memfs_dupfd(int fd, bool rewind)
{
	int newfd = dup(fd);
	if (rewind)
	{
		lseek(fd, 0, SEEK_SET);
	}
	return newfd;
}
