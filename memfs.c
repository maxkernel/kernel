#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <aul/string.h>
#include <aul/exception.h>
#include <kernel.h>
#include <kernel-priv.h>


#ifndef MEMFS
	#define MEMFS		"memfs"
#endif

#ifndef INSTALL
	#define MOUNT_PATH	("/tmp/" MEMFS)
#else
	#define MOUNT_PATH	(INSTALL "/" MEMFS)
#endif

#define MOUNT_NAME		"maxkernel_memfs"
#define MOUNT_TYPE		"ramfs"


bool memfs_init(exception_t ** err)
{
	memfs_destroy(NULL);
	mkdir(MOUNT_PATH, S_IRWXU | S_IRWXG | S_IRWXO);

	int success = mount(MOUNT_NAME, MOUNT_PATH, MOUNT_TYPE, MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL);
	if (success != 0 && err != NULL)
	{
		exception_set(err, errno, "Count not mount %s of type %s on point %s: %s", MOUNT_NAME, MOUNT_TYPE, MOUNT_PATH, strerror(errno));
		return false;
	}

	return true;
}

bool memfs_destroy(exception_t ** err)
{
	int success = umount2(MOUNT_PATH, MNT_DETACH);
	if (success != 0 && err != NULL)
	{
		exception_set(err, errno, "Count not unmount %s on point %s: %s", MOUNT_NAME, MOUNT_PATH, strerror(errno));
		return false;
	}

	return true;
}

int memfs_newfd(const char * name, int oflags)
{
	string_t path = path_join(MOUNT_PATH, name);
	return open(path.string, oflags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}

void memfs_delete(const char * name)
{
	string_t path = path_join(MOUNT_PATH, name);
	unlink(path.string);
}

int memfs_orphanfd()
{
	string_t path = string_new("orphan-%" PRIu64 "-%d", kernel_timestamp(), rand());
	int fd = memfs_newfd(path.string, O_RDWR | O_CREAT | O_EXCL);
	memfs_delete(path.string);

	if (fd == -1)
	{
		LOGK(LOG_ERR, "Could not create memfs orphanfd: %s", strerror(errno));
		return -1;
	}

	return fd;
}

void memfs_closefd(int fd)
{
	if (close(fd) == -1)
	{
		LOGK(LOG_ERR, "Could not close memfs fd %d: %s", fd, strerror(errno));
	}
}

int memfs_dupfd(int fd)
{
	int newfd = dup(fd);
	if (newfd == -1)
	{
		LOGK(LOG_ERR, "Could not duplicate memfs fd %d: %s", fd, strerror(errno));
		return -1;
	}

	memfs_rewindfd(newfd);
	return newfd;
}

off_t memfs_sizefd(int fd)
{
	// Get current pos
	off_t cur1 = lseek(fd, 0, SEEK_CUR);

	// Seek to end and get pos
	off_t len = lseek(fd, 0, SEEK_END);

	// Now seek back to where we were
	off_t cur2 = lseek(fd, cur1, SEEK_SET);

	if (len < 0 || cur1 != cur2)
	{
		LOGK(LOG_ERR, "Could not read size of memfs fd %d. Descriptor might be corrupted", fd);
		return 0;
	}

	return len;
}

void memfs_syncfd(int fd)
{
	fsync(fd);
}

void memfs_setseekfd(int fd, off_t position)
{
	lseek(fd, position, SEEK_SET);
}

off_t memfs_getseekfd(int fd)
{
	return lseek(fd, 0, SEEK_CUR);
}

void memfs_rewindfd(int fd)
{
	if (lseek(fd, 0, SEEK_SET) != 0)
	{
		LOGK(LOG_ERR, "Could not rewind memfs fd %d: %s", fd, strerror(errno));
	}
}
