#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <kernel.h>


MOD_VERSION("0.9");
MOD_AUTHOR("Andrew Klofas - andrew.klofas@senseta.com");
MOD_DESCRIPTION("Network utility tools module (signal strength, etc.)");
MOD_INIT(module_init);

BLK_OUTPUT(STATIC, strength, S_DOUBLE);
BLK_ONUPDATE(STATIC, block_update);


#define SIGNAL_DIR			"/sys/class/net"
#define SIGNAL_LINK			"wireless/link"
#define SIGNAL_MAX			100.0

static const char * sigpath = NULL;

void block_update(void * object)
{
	if (sigpath == NULL)
	{
		return;
	}

	FILE * sigfd = fopen(sigpath, "r");
	if (sigfd != NULL)
	{
		double strength = 0.0;
		fscanf(sigfd, "%lf", &strength);
		fclose(sigfd);

		strength /= SIGNAL_MAX;
		OUTPUT(strength, &strength);
	}
}

void module_init()
{
	DIR * dirp = opendir(SIGNAL_DIR);
	if (dirp != NULL)
	{
		struct dirent * ent;
		while ((ent = readdir(dirp)) != NULL)
		{
			string_t path = string_new(SIGNAL_DIR "/%s/" SIGNAL_LINK, ent->d_name);
			struct stat statbuf;

			ZERO(statbuf);
			if (stat(path.string, &statbuf) == 0 && S_ISREG(statbuf.st_mode))
			{
				sigpath = string_copy(&path);
			}
		}
	}
	closedir(dirp);
}
