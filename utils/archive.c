#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <zlib.h>
#include <libtar.h>

#include <aul/common.h>
#include <aul/string.h>


static bool extract_tar_gz(char * path, char * dir)
{
	gzFile fds[1];
	size_t n = 0;

	int gzopen_frontend(const char * pathname, int oflags, ...)
	{
		if (n >= nelems(fds))
		{
			errno = ENOMEM;
			return -1;
		}


		char * gzoflags;
		switch (oflags & O_ACCMODE)
		{
			case O_WRONLY:	gzoflags = "w";		break;
			case O_RDONLY:	gzoflags = "r";		break;
			case O_RDWR:
			default:
				errno = EINVAL;
				return -1;
		}

		size_t i = n++;
		fds[i] = gzopen(pathname, gzoflags);
		if (!fds[i])
		{
			errno = ENOMEM;
			return -1;
		}

		return i;
	}

	int gzclose_frontend(int fd)
	{
		if (fd < 0 || fd >= n)
		{
			errno = EINVAL;
			return -1;
		}

		return gzclose(fds[fd]);
	}

	ssize_t gzread_frontend(int fd, void * buf, size_t len)
	{
		if (fd < 0 || fd >= n)
		{
			errno = EINVAL;
			return -1;
		}

		return gzread(fds[fd], buf, len);
	}

	ssize_t gzwrite_frontend(int fd, const void * buf, size_t len)
	{
		if (fd < 0 || fd >= n)
		{
			errno = EINVAL;
			return -1;
		}

		return gzwrite(fds[fd], buf, len);
	}

	TAR * t;
	tartype_t type = {
		.openfunc = gzopen_frontend,
		.closefunc = gzclose_frontend,
		.readfunc = gzread_frontend,
		.writefunc = gzwrite_frontend,
	};

	if (tar_open(&t, path, &type, O_RDONLY, 0, TAR_GNU) != 0)
	{
		return -1;
	}

	while (th_read(t) == 0)
	{
		string_t name = string_new("%s/%s", dir, th_get_pathname(t));
		if (tar_extract_file(t, name.string) != 0)
		{
			return false;
		}
	}

	if (tar_close(t) != 0)
	{
		return false;
	}
	
	return true;
}

bool extract(char * path, char * dir)
{
	if (strsuffix(path, ".tar.gz") || strsuffix(path, ".stuff"))
	{
		return extract_tar_gz(path, dir);
	}

	return false;
}

