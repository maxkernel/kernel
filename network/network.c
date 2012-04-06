#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <kernel.h>


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

bool module_init()
{
	DIR * dirp = opendir(SIGNAL_DIR);
	if (dirp != NULL)
	{
		struct dirent * ent;
		while ((ent = readdir(dirp)) != NULL)
		{
			string_t path = string_new(SIGNAL_DIR "/%s/" SIGNAL_LINK, ent->d_name);

			struct stat statbuf;
			memset(&statbuf, 0, sizeof(statbuf));

			if (stat(path.string, &statbuf) == 0 && S_ISREG(statbuf.st_mode))
			{
				sigpath = string_copy(&path);
				break;
			}
		}
	}
	closedir(dirp);

	return true;
}


module_name("Network Health Monitor");
module_version(1,0,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Network utility tools to measure signal strength, etc.");
module_oninitialize(module_init);

block_output(	static, 	strength, 		'd', 	"The strength of the wireless link (0-100)");
block_onupdate(	static, 	block_update);
