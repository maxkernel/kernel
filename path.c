#include <sys/stat.h>

#include <aul/common.h>
#include <aul/string.h>
#include <kernel.h>

static char * pathlist[PATH_MAXPATHS] = {0};
static char pathbuf[PATH_BUFSIZE] = {0};

static size_t pathlist_next = 0;
static char * pathbuf_next = pathbuf;


static inline bool file_exists(const char * path)
{
	struct stat buf;
	int i = stat(path, &buf);
	return i == 0 && S_ISREG(buf.st_mode);
}

static inline bool dir_exists(const char * path)
{
	struct stat buf;
	int i = stat(path, &buf);
	return i == 0 && S_ISDIR(buf.st_mode);
}

bool path_set(const char * newpath, exception_t ** err)
{
	// Sanity check
	if (exception_check(err))
	{
		return false;
	}

	memset(pathlist, 0, sizeof(pathlist));
	memset(pathbuf, 0, sizeof(pathbuf));
	pathbuf_next = pathbuf;
	pathlist_next = 0;

	if (newpath == NULL)
	{
		return true;
	}

	return path_append(newpath, err);
}

bool path_append(const char * path, exception_t ** err)
{
	// Sanity check
	if (exception_check(err) || path == NULL)
	{
		return false;
	}

	const char * start = path;
	ssize_t length = strlen(path);

	while (length > 0)
	{
		// Get the length of the next path segment
		char * end = strchr(start, ':');
		size_t plen = (end == NULL)? length : end - start;

		// Make sure there is enough room in the path list
		if (pathlist_next >= PATH_MAXPATHS)
		{
			// Not enough room!
			exception_set(err, ENOMEM, "Could not append path, not enough room! Consider increasing PATH_MAXPATHS (currently %d)", PATH_MAXPATHS);
			return false;
		}

		// Make sure there is enough space in the path buffer
		if ((plen + 1) > (PATH_BUFSIZE - (pathbuf_next - pathbuf)))
		{
			// Not enough room!
			exception_set(err, ENOMEM, "Could not append path, not enough room! Consider increasing PATH_BUFSIZE (currently %d)", PATH_BUFSIZE);
			return false;
		}

		// Copy the segment into the path buffer
		memcpy(pathbuf_next, start, plen);
		pathbuf_next[plen++] = '\0';

		// Update the pointers
		pathlist[pathlist_next++] = pathbuf_next;
		pathbuf_next += plen;
		length -= plen;
		start += plen;
	}

	return true;
}

// Returns the directory path prefix where the file can be found
const char * path_resolve(const char * name, ptype_t type)
{
	string_t file = string_new("%s", name);
	if (type & P_MODULE)
	{
		 if (!strsuffix(file.string, ".mo"))
		 {
			// We are looking for a module, add an '.mo' to the name
			string_append(&file, ".mo");
		 }

		 type = P_FILE;
	}

	if (unlikely(file.string[0] == '/'))
	{
		// Absolute path
		if (((type & P_FILE) && file_exists(file.string)) || ((type & P_DIRECTORY) && dir_exists(file.string)))
		{
			// Return an empty string to denote that no prefix is required
			return "";
		}
	}
	else
	{
		for (size_t i = 0; pathlist[i] != NULL && i < pathlist_next; i++)
		{
			// Relative path
			string_t test = path_join(pathlist[i], file.string);
			if (((type & P_FILE) && file_exists(test.string)) || ((type & P_DIRECTORY) && dir_exists(test.string)))
			{
				// Return an empty string to denote that no prefix is required
				return pathlist[i];
			}
		}
	}

	return NULL;
}

string_t path_join(const char * prefix, const char * file)
{
	bool slash_needed = !strprefix(file, "/") && !strsuffix(prefix, "/");
	return string_new("%s%s%s", prefix, (slash_needed)? "/" : "", file);
}
