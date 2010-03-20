#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <zlib.h>

#include <aul/string.h>
#include <aul/log.h>


#define ZLIB_BUFSIZE		16384
#define FILEOPEN(p)			open(p, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
#define FILEOPENHEADER(fd)														\
	do {																		\
		char _timebuf[50];														\
		time_t _now = time(NULL);												\
		strftime(_timebuf, sizeof(_timebuf), "%F.%H.%M.%S", localtime(&_now));	\
		String _str = string_new("\n[%s] Log file opened\n", _timebuf);			\
		write(fd, _str.string, _str.length);									\
	} while (0)


static inline const char * level2string(Level level)
{
	char * levelstr = "Unkno";
	if	((level & LEVEL_FATAL) > 0)			levelstr = "FATAL";
	else if	((level & LEVEL_ERROR) > 0)		levelstr = "Error";
	else if ((level & LEVEL_WARNING) > 0)	levelstr = "Warn";
	else if ((level & LEVEL_INFO) > 0)		levelstr = "Info";
	else if ((level & LEVEL_DEBUG) > 0)		levelstr = "Debug";

	return levelstr;
}

static boolean file_exists(const char * path)
{
	struct stat buf;
	int i = stat(path, &buf);
	return i == 0 && S_ISREG(buf.st_mode);
}

static off_t file_size(const char * path)
{
	struct stat buf;
	int i = stat(path, &buf);
	return i==0? buf.st_size : 0;
}

static void move_archive(const char * prefix, int arnum)
{
	String oldpath = string_new("%s.%d.gz", prefix, arnum);
	if (!file_exists(oldpath.string))
	{
		return;
	}

	move_archive(prefix, arnum+1);

	String newpath = string_new("%s.%d.gz", prefix, arnum+1);
	rename(oldpath.string, newpath.string);
}

static boolean compress_archive(const char * prefix, int arnum)
{
	String oldpath = string_new("%s.%d", prefix, arnum);
	if (!file_exists(oldpath.string))
	{
		//nothing to do
		return TRUE;
	}
	String newpath = string_new("%s.%d.gz", prefix, arnum+1);

	//perform compression
	{
		FILE * src = fopen(oldpath.string, "rb");
		gzFile dest = gzopen(newpath.string, "wb");

		if (src == NULL || dest == NULL)
		{
			return FALSE;
		}

		unsigned char buf[ZLIB_BUFSIZE];
		do
		{
			size_t bytes = fread(buf, 1, sizeof(buf), src);
			if (ferror(src))
			{
				fclose(src);
				gzclose(dest);
				return FALSE;
			}

			if (gzwrite(dest, buf, bytes) != bytes)
			{
				fclose(src);
				gzclose(dest);
				return FALSE;
			}
		} while (!feof(src));

		fclose(src);
		gzflush(dest, Z_FINISH);
		gzclose(dest);
	}

	unlink(oldpath.string);
	return TRUE;
}

static void archive(const char * path)
{
	//move old archives
	move_archive(path, 2);
	if (!compress_archive(path, 1))
	{
		log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Could not compress old log file %s. Log data has been lost.", path);
	}

	String newpath = string_new("%s.1", path);
	rename(path, newpath.string);
}


struct log_file
{
	const char * path;
	int fd;
	off_t size;
};

static void log_print(Level level, const char * domain, uint64_t milliseconds, const char * message, void * userdata)
{
	fprintf(stdout, "%s (%-5s) %lld - %s\n", domain, level2string(level), milliseconds, message);
}

static void log_filewrite(Level level, const char * domain, uint64_t milliseconds, const char * message, void * userdata)
{
	struct log_file * data = userdata;

	if (data->fd == -1)
	{
		return;
	}

	String buf = string_new("<%s, %s, %lld> %s\n", domain, level2string(level), milliseconds, message);
	ssize_t wrote = write(data->fd, buf.string, buf.length);

	if (wrote == -1)
	{
		close(data->fd);
		data->fd = -1;
		log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Could not write last log message to file. File logging to %s has been disabled for this session.", data->path);
		return;
	}

	data->size += wrote;
	if (data->size > AUL_LOG_MAXFILESIZE)
	{
		//log file has become too big. Close it, archive it, and open a new one
		close(data->fd);
		archive(data->path);
		data->fd = FILEOPEN(data->path);
		FILEOPENHEADER(data->fd);
		data->size = 0;
	}
}

static void log_fileclose(void * userdata)
{
	struct log_file * data = userdata;

	if (data->fd != -1)
	{
		close(data->fd);
	}

	FREES(data->path);
	FREE(data);
}



struct
{
	log_f function;
	logclose_f closer;
	void * userdata;
} log_listeners[AUL_LOG_MAXLISTENERS] = {{log_print, NULL, NULL}, {0}};

void log_destroy()
{
	size_t i=0;
	for (; i<AUL_LOG_MAXLISTENERS; i++)
	{
		if (log_listeners[i].closer != NULL)
		{
			log_listeners[i].closer(log_listeners[i].userdata);
			log_listeners[i].function = NULL;
			log_listeners[i].closer = NULL;
			log_listeners[i].userdata = NULL;
		}
	}
}

boolean log_openfile(const char * path, Error ** err)
{
	if (error_check(err))
	{
		log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Error already set in function log_open");
		return FALSE;
	}
	
	int fd = FILEOPEN(path);
	if (fd == -1)
	{
		if (err != NULL)
		{
			*err = error_new(errno, strerror(errno));
		}
		return FALSE;
	}
	FILEOPENHEADER(fd);

	struct log_file * data = malloc0(sizeof(struct log_file));
	data->path = strdup(path);
	data->size = file_size(path);
	data->fd = fd;

	log_addlistener(log_filewrite, log_fileclose, data);
	return TRUE;
}

void log_dispatch(Level level, const char * domain, const char * fmt, va_list args)
{
	//get time (milliseconds) since first log_dispatch
	static struct timeval logp_start = {0};
	if (logp_start.tv_sec == 0)
	{
		gettimeofday(&logp_start, NULL);
	}
	struct timeval now;
	gettimeofday(&now, NULL);
	uint64_t diff = (now.tv_sec - logp_start.tv_sec) * 1000LL + (now.tv_usec - logp_start.tv_usec) / 1000LL;

	//construct string
	String str;
	string_clear(&str);
	string_vappend(&str, fmt, args);
	
	size_t i=0;
	for (; i<AUL_LOG_MAXLISTENERS; i++)
	{
		if (log_listeners[i].function != NULL)
		{
			log_listeners[i].function(level, domain, diff, str.string, log_listeners[i].userdata);
		}
	}

	if ((level & LEVEL_FATAL) != 0)
	{
		exit(-1);
	}
}

void log_addlistener(log_f listener, logclose_f closer, void * userdata)
{
	size_t i = 0;
	for (; i<AUL_LOG_MAXLISTENERS; i++)
	{
		if (log_listeners[i].function == NULL)
		{
			log_listeners[i].function = listener;
			log_listeners[i].closer = closer;
			log_listeners[i].userdata = userdata;
			return;
		}
	}

	log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Could not add log listener. Listener list is full!");
}

void log_removelistener(log_f listener)
{
	size_t i = 0;
	for (; i<AUL_LOG_MAXLISTENERS; i++)
	{
		if (log_listeners[i].function == listener)
		{
			log_listeners[i].function = NULL;
			log_listeners[i].closer = NULL;
			log_listeners[i].userdata = NULL;
			return;
		}
	}
}

void log_setdefault(log_f listener, logclose_f closer, void * userdata)
{
	log_listeners[0].function = listener;
	log_listeners[0].closer = closer;
	log_listeners[0].userdata = userdata;
}
