#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <kernel.h>


#define SIGNAL_DIR			"/sys/class/net"
#define SIGNAL_LINK			"wireless/link"
#define SIGNAL_MAX			100.0

void * wifi_new(const char * name)
{
	string_t path = string_new("%s/%s/%s", SIGNAL_DIR, name, SIGNAL_LINK);

	struct stat statbuf;
	memset(&statbuf, 0, sizeof(statbuf));
	if (stat(path.string, &statbuf) != 0 || !S_ISREG(statbuf.st_mode))
	{
		LOG(LOG_ERR, "No such wifi device %s", name);
		return NULL;
	}

	return string_copy(&path);
}

void wifi_update(void * object)
{
	const char * path = object;
	if (path == NULL)
	{
		return;
	}

	FILE * fp = fopen(path, "r");
	if (fp != NULL)
	{
		double strength = 0.0;
		fscanf(fp, "%lf", &strength);
		fclose(fp);

		strength /= SIGNAL_MAX;
		output(strength, &strength);
	}
}


module_name("Network Health Monitor");
module_version(1,0,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Network utility tools to measure signal strength, etc.");

define_block(wifi, "Wifi device utility", wifi_new, "s", "(1) Wifi device [eg. wlan0]");
block_output(	wifi, 	strength, 		'd', 	"The strength of the wireless link (0-100)");
block_onupdate(	wifi, 	wifi_update);
