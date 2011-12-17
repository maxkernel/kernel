#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <bfd.h>

#include <glib.h>

#include "kernel.h"
#include "kernel-priv.h"

static char path[PATH_MAX_SIZE] = {0};

static GRegex * meta_regex = NULL;

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

void setpath(const char * newpath)
{
	path[0] = 0;
	appendpath(newpath);
}

void appendpath(const char * newentry)
{
	size_t pathlen = strlen(path);
	size_t newlen = strlen(newentry);

	if (pathlen + newlen + 2 > PATH_MAX_SIZE)
	{
		LOGK(LOG_ERR, "Max path size of %d exceeded.", PATH_MAX_SIZE);
		return;
	}

	path[pathlen] = ':';
	memcpy(path + pathlen + 1, newentry, newlen + 1);
}

const char * getpath()
{
	return path;
}

const char * resolvepath(const char * name, int etype)
{
	const char * filepath = NULL;

	if (name[0] == '/')
	{
		//absolute path
		if ((etype & PATH_FILE && file_exists(name)) || (etype & PATH_DIRECTORY && dir_exists(name)))
		{
			filepath = strdup(name);
		}
	}
	else
	{
		string_t abspath;
		char pathbuf[PATH_MAX_SIZE];
		memcpy(pathbuf, path, sizeof(pathbuf));

		char * start = pathbuf, * end;
		do
		{
			end = strchr(start, ':');
			if (end != NULL)
			{
				*end++ = '\0';
			}

			string_clear(&abspath);
			string_append(&abspath, "%s/%s", start, name);

			if ((etype & PATH_FILE && file_exists(abspath.string)) || (etype & PATH_DIRECTORY && dir_exists(abspath.string)))
			{
				filepath = string_copy(&abspath);
				break;
			}

			start = end;
		} while (start != NULL);
	}

	if (filepath == NULL)
		LOGK(LOG_WARN, "Resolvepath failed! Could not find %s. Check the path variable", name);

	return filepath;
}

static block_t * getblk(list_t * list, char * name)
{
	list_t * pos;
	block_t * blk;


	list_foreach(pos, list)
	{
		blk = list_entry(pos, block_t, module_list);

		if (strcmp(name, blk->name) == 0)
		{
			return blk;
		}
	}

	// block does not exist, add it to list
	blk = malloc0(sizeof(block_t));
	blk->kobject.class_name = "Block";
	blk->name = strdup(name);
	LIST_INIT(&blk->inputs);
	LIST_INIT(&blk->outputs);

	list_add(list, &blk->module_list);
	return blk;
}

meta_t * meta_parse(const char * path)
{	
	bfd * abfd;
	asection * sect;
	
	meta_t * meta = NULL;
	
	char * buf;
	int buflen = 0;
	int bufi = 0;
	
	abfd = bfd_openr(path, NULL);
	
	if (abfd == NULL)
	{
		LOGK(LOG_ERR, "Error reading meta information on %s: %s", path, bfd_errmsg(bfd_get_error()));
		return NULL;
	}
	
	if (!bfd_check_format(abfd, bfd_object))
	{
		LOGK(LOG_ERR, "Wrong module format %s: %s", path, bfd_errmsg(bfd_get_error()));
		goto end;
	}
	
	if (meta_regex == NULL)
	{
		GError * err = NULL;
		meta_regex = g_regex_new("^([[:alpha:]\\.]+)\\(\"([^\"]*)\"(?:, \"([^\"]*)\")?(?:, \"([^\"]*)\")?(?:, \"([^\"]*)\")?(?:, \"([^\"]*)\")?(?:, \"([^\"]*)\")?,?\\)$", G_REGEX_OPTIMIZE, 0, &err);
		
		if (err != NULL)
		{
			LOGK(LOG_FATAL, "Error creating RegEx: %s", err->message);
			g_error_free(err);
		}
	}
	
	//build meta
	meta = malloc0(sizeof(meta_t));
	LIST_INIT(&meta->dependencies);
	LIST_INIT(&meta->syscalls);
	LIST_INIT(&meta->cfgentries);
	LIST_INIT(&meta->calentries);
	LIST_INIT(&meta->blocks);

	sect = bfd_get_section_by_name(abfd, ".meta");
	if (sect == 0)
	{
		//section doesn't exist, nothing to do
		goto end;
	}
	
	buflen = bfd_get_section_size(sect);
	buf = malloc0(buflen);
	bfd_get_section_contents(abfd, sect, buf, 0, buflen);
	
	while (bufi < buflen)
	{
		GMatchInfo * info;
		
		g_regex_match(meta_regex, buf+bufi, 0, &info);
		if (g_match_info_matches(info))
		{
			char * type = g_match_info_fetch(info, 1);
			
			if (strcmp(type, M_SYSCALL) == 0)
			{
				syscall_t * syscall = malloc0(sizeof(syscall_t));
				syscall->name = g_match_info_fetch(info, 2);
				syscall->sig = g_match_info_fetch(info, 3);
				syscall->desc = g_match_info_fetch(info, 4);
				
				list_add(&meta->syscalls, &syscall->module_list);
			}
			else if (strcmp(type, M_CFGPARAM) == 0)
			{
				cfgentry_t * cfg = malloc0(sizeof(cfgentry_t));
				cfg->name = g_match_info_fetch(info, 2);

				char * sig = g_match_info_fetch(info, 3);
				cfg->type = sig[0];
				g_free(sig);

				cfg->desc = g_match_info_fetch(info, 4);

				list_add(&meta->cfgentries, &cfg->module_list);
			}
			else if (strcmp(type, M_CALPARAM) == 0)
			{
				calentry_t * cal = malloc0(sizeof(calentry_t));
				cal->name = g_match_info_fetch(info, 2);
				cal->sig = g_match_info_fetch(info, 3);
				cal->desc = g_match_info_fetch(info, 4);
				
				list_add(&meta->calentries, &cal->module_list);
			}
			else if (strcmp(type, M_CALUPDATE) == 0)
			{
				meta->calupdate = g_match_info_fetch(info, 2);
			}
			else if (strcmp(type, M_CALPREVIEW) == 0)
			{
				meta->calpreview = g_match_info_fetch(info, 2);
			}
			else if (strcmp(type, M_VERSION) == 0)
			{
				meta->version = g_match_info_fetch(info, 2);
			}
			else if (strcmp(type, M_AUTHOR) == 0)
			{
				meta->author = g_match_info_fetch(info, 2);
			}
			else if (strcmp(type, M_DESC) == 0)
			{
				meta->description = g_match_info_fetch(info, 2);
			}
			else if (strcmp(type, M_DEP) == 0)
			{
				int i = 2;
				char * depname;
				
				while ((depname = g_match_info_fetch(info, i++)) != NULL)
				{
					dependency_t * dep = malloc0(sizeof(dependency_t));
					dep->modname = depname;
					list_add(&meta->dependencies, &dep->module_list);
				}
			}
			else if (strcmp(type, M_PREACT) == 0)
			{
				if (meta->preactivate != NULL)
				{
					LOGK(LOG_WARN, "Multiple preactivation functions defined. Will only calling one");
				}
				else
				{
					meta->preactivate = g_match_info_fetch(info, 2);
				}
			}
			else if (strcmp(type, M_POSTACT) == 0)
			{
				if (meta->postactivate != NULL)
				{
					LOGK(LOG_WARN, "Multiple postactivation functions defined. Will only calling one");
				}
				else
				{
					meta->postactivate = g_match_info_fetch(info, 2);
				}
			}
			else if (strcmp(type, M_INIT) == 0)
			{
				if (meta->initialize != NULL)
				{
					LOGK(LOG_WARN, "Multiple initialization functions defined. Will only calling one");
				}
				else
				{
					meta->initialize = g_match_info_fetch(info, 2);
				}
			}
			else if (strcmp(type, M_DESTROY) == 0)
			{
				if (meta->destroy != NULL)
				{
					LOGK(LOG_WARN, "Multiple destroy functions defined. Only calling one");
				}
				else
				{
					meta->destroy = g_match_info_fetch(info, 2);
				}
			}
			else if (strcmp(type, M_BLOCK) == 0)
			{
				char * name = g_match_info_fetch(info, 2);
				block_t * blk = getblk(&meta->blocks, name);
				g_free(name);

				blk->new_name = g_match_info_fetch(info, 3);
				blk->new_sig = g_match_info_fetch(info, 4);
				blk->desc = g_match_info_fetch(info, 5);
			}
			else if (strcmp(type, M_ONUPDATE) == 0)
			{
				char * name = g_match_info_fetch(info, 2);
				block_t * blk = getblk(&meta->blocks, name);
				g_free(name);

				blk->onupdate_name = g_match_info_fetch(info, 3);
			}
			else if (strcmp(type, M_ONDESTROY) == 0)
			{
				char * name = g_match_info_fetch(info, 2);
				block_t * blk = getblk(&meta->blocks, name);
				g_free(name);

				blk->ondestroy_name = g_match_info_fetch(info, 3);
			}
			else if (strcmp(type, M_INPUT) == 0)
			{
				char * name = g_match_info_fetch(info, 2);
				block_t * blk = getblk(&meta->blocks, name);
				g_free(name);

				bio_t * in = malloc0(sizeof(bio_t));
				in->name = g_match_info_fetch(info, 3);

				char * sig = g_match_info_fetch(info, 4);
				in->sig = sig[0];
				g_free(sig);

				in->desc = g_match_info_fetch(info, 5);

				list_add(&blk->inputs, &in->block_list);
			}
			else if (strcmp(type, M_OUTPUT) == 0)
			{
				char * name = g_match_info_fetch(info, 2);
				block_t * blk = getblk(&meta->blocks, name);
				g_free(name);

				bio_t * out = malloc0(sizeof(bio_t));
				out->name = g_match_info_fetch(info, 3);

				char * sig = g_match_info_fetch(info, 4);
				out->sig = sig[0];
				g_free(sig);

				out->desc = g_match_info_fetch(info, 5);

				list_add(&blk->outputs, &out->block_list);
			}
			else
			{
				LOGK(LOG_WARN, "Unknown meta type encountered %s", type);
			}
			
			g_free(type);
		}
		else if (strlen(buf+bufi) > 0)
		{
			LOGK(LOG_ERR, "Malformed meta string: %s", buf+bufi);
		}
		
		g_match_info_free(info);
		bufi += strlen(buf+bufi)+1;
	}
	
	g_free(buf);
	
end:
	bfd_close(abfd);
	return meta;
}

