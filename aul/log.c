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
#include <aul/mutex.h>
#include <aul/log.h>


#define ZLIB_BUFSIZE		16384
#define FILEOPEN(p)			open(p, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
#define FILEOPENHEADER(fd) \
	({ \
		char _timebuf[50];														\
		time_t _now = time(NULL);												\
		strftime(_timebuf, sizeof(_timebuf), "%F.%H.%M.%S", localtime(&_now));	\
		string_t _str = string_new("\n[%s] Log file opened\n", _timebuf);		\
		write(fd, _str.string, _str.length);									\
	})


static inline const char * level2string(level_t level)
{
	char * levelstr = "Unkno";
	if	((level & LEVEL_FATAL) > 0)			levelstr = "FATAL";
	else if	((level & LEVEL_ERROR) > 0)		levelstr = "Error";
	else if ((level & LEVEL_WARNING) > 0)	levelstr = "Warn";
	else if ((level & LEVEL_INFO) > 0)		levelstr = "Info";
	else if ((level & LEVEL_DEBUG) > 0)		levelstr = "Debug";

	return levelstr;
}

static bool file_exists(const char * path)
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
	string_t oldpath = string_new("%s.%d.gz", prefix, arnum);
	if (!file_exists(oldpath.string))
	{
		return;
	}

	move_archive(prefix, arnum+1);

	string_t newpath = string_new("%s.%d.gz", prefix, arnum+1);
	rename(oldpath.string, newpath.string);
}

static bool compress_archive(const char * prefix, int arnum)
{
	string_t oldpath = string_new("%s.%d", prefix, arnum);
	if (!file_exists(oldpath.string))
	{
		//nothing to do
		return true;
	}
	string_t newpath = string_new("%s.%d.gz", prefix, arnum+1);

	//perform compression
	{
		FILE * src = fopen(oldpath.string, "r");
		gzFile dest = gzopen(newpath.string, "w");

		if (src == NULL || dest == NULL)
		{
			return false;
		}

		unsigned char buf[ZLIB_BUFSIZE];
		do
		{
			size_t bytes = fread(buf, 1, sizeof(buf), src);
			if (ferror(src))
			{
				fclose(src);
				gzclose(dest);
				return false;
			}

			if (gzwrite(dest, buf, bytes) != bytes)
			{
				fclose(src);
				gzclose(dest);
				return false;
			}
		} while (!feof(src));

		fclose(src);
		gzflush(dest, Z_FINISH);
		gzclose(dest);
	}

	unlink(oldpath.string);
	return true;
}

static void archive(const char * path)
{
	//move old archives
	move_archive(path, 2);
	if (!compress_archive(path, 1))
	{
		log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Could not compress old log file %s. Log data has been lost.", path);
	}

	string_t newpath = string_new("%s.1", path);
	rename(path, newpath.string);
}


typedef struct
{
	char * path;
	int fd;
	off_t size;
} logfile_t;

static void log_print(level_t level, const char * domain, uint64_t milliseconds, const char * message, void * userdata)
{
	fprintf(stdout, "%s (%-5s) %" PRIu64 " - %s\n", domain, level2string(level), milliseconds, message);
}


// TODO - put a mutex around accessing this struct
static struct
{
	log_f function;
	logclose_f closer;
	void * userdata;
} log_listeners[AUL_LOG_MAXLISTENERS] = {{log_print, NULL, NULL}, {0}};

void log_destroy()
{
	for (size_t i = 0; i < AUL_LOG_MAXLISTENERS; i++)
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

bool log_addfd(int fd, exception_t ** err)
{
	void log_filewrite(level_t level, const char * domain, uint64_t milliseconds, const char * message, void * userdata)
	{
		int * fd = userdata;
		if (*fd == -1)
		{
			return;
		}

		string_t buf = string_new("<%s, %s, %" PRIu64 "> %s\n", domain, level2string(level), milliseconds, message);
		ssize_t wrote = write(*fd, buf.string, buf.length);

		if (wrote == -1)
		{
			close(*fd);
			*fd = -1;
			log_write(LEVEL_WARNING, AUL_LOG_DOMAIN, "Could not write last log message to file pointer. File logging to the pointer has been disabled for this session.");
			return;
		}
	}

	void log_fileclose(void * userdata)
	{
		int * fd = userdata;
		if (*fd != -1)
		{
			close(*fd);
		}

		free(fd);
	}

	// Sanity check
	if (exception_check(err) || fd < 0)
	{
		return false;
	}

	FILEOPENHEADER(fd);

	int * data = malloc(sizeof(int));
	*data = fd;
	log_addlistener(log_filewrite, log_fileclose, data);

	return true;
}

bool log_openfile(const char * path, exception_t ** err)
{
	void log_filewrite(level_t level, const char * domain, uint64_t milliseconds, const char * message, void * userdata)
	{
		logfile_t * data = userdata;

		if (data->fd == -1)
		{
			return;
		}

		string_t buf = string_new("<%s, %s, %" PRIu64 "> %s\n", domain, level2string(level), milliseconds, message);
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
			// The log file has become too big. Close it, archive it, and open a new one
			close(data->fd);
			archive(data->path);
			data->fd = FILEOPEN(data->path);
			FILEOPENHEADER(data->fd);
			data->size = 0;
		}
	}

	void log_fileclose(void * userdata)
	{
		logfile_t * data = userdata;

		if (data->fd != -1)
		{
			close(data->fd);
		}

		free(data->path);
		free(data);
	}

	// Sanity check
	if (exception_check(err))
	{
		return false;
	}


	int fd = FILEOPEN(path);
	if (fd == -1)
	{
		exception_set(err, errno, "Count not open log file: %s", strerror(errno));
		return false;
	}
	FILEOPENHEADER(fd);

	logfile_t * data = malloc(sizeof(logfile_t));
	memset(data, 0, sizeof(logfile_t));
	data->path = strdup(path);
	data->size = file_size(path);
	data->fd = fd;
	log_addlistener(log_filewrite, log_fileclose, data);

	return true;
}

void log_dispatch(level_t level, const char * domain, const char * fmt, va_list args)
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
	string_t str;
	string_clear(&str);
	string_vappend(&str, fmt, args);
	
	for (size_t i = 0; i < AUL_LOG_MAXLISTENERS; i++)
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
	for (size_t i = 0; i < AUL_LOG_MAXLISTENERS; i++)
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
	for (size_t i = 0; i < AUL_LOG_MAXLISTENERS; i++)
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
