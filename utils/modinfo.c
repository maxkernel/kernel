#include <stdio.h>
#include <string.h>
#include <glib.h>

#include <kernel.h>
#include "kernel-priv.h"	// TODO - is this needed?

/*
static void log_write(const char * domain, GLogLevelFlags level, const char * message, gpointer userdata)
{
	gint level_type = level & (G_LOG_LEVEL_ERROR|G_LOG_LEVEL_CRITICAL|G_LOG_LEVEL_WARNING|G_LOG_LEVEL_INFO|G_LOG_LEVEL_DEBUG);
	gchar * level_txt = "Unknown";
	switch (level_type)
	{
		case G_LOG_LEVEL_ERROR:		level_txt = "Fatal";	break;
		case G_LOG_LEVEL_CRITICAL:	level_txt = "Error";	break;
		case G_LOG_LEVEL_WARNING:	level_txt = "Warn";		break;
		case G_LOG_LEVEL_INFO:		level_txt = "";			break;
		case G_LOG_LEVEL_DEBUG:		return;	//don't print log messages
	}

	if (strlen(level_txt) > 0)
	{
		g_print("(%s) %s\n", level_txt, message);
	}
	else
	{
		g_print("%s\n", strstr(message, ":")+1);
	}
}
*/

int main(int argc, char ** argv)
{
	//set up logging
	//g_log_set_default_handler(log_write, NULL);
	path_set(".");
	const char * prefix = path_resolve(argv[1]);
	if (prefix == NULL)
	{
		return EXIT_FAILURE;
	}

	string_t file = string_new("%s/%s", prefix, argv[1]);
	meta_t * meta = meta_parse(file.string);

	GHashTableIter itr;
	List * next;
	block_t * block;
	
	LOG(LOG_INFO, "Module: %s", file.string);
	LOG(LOG_INFO, "==================================");
	LOG(LOG_INFO, "Author:\t %s", meta->author);
	LOG(LOG_INFO, "Version:\t %s", meta->version);
	LOG(LOG_INFO, "Pre-Activater:\t %s", meta->preactivate);
	LOG(LOG_INFO, "Post-Activator: %s", meta->postactivate);
	LOG(LOG_INFO, "Initializer:\t %s", meta->initialize);
	LOG(LOG_INFO, "Description:\t %s\n", meta->description);

	if (list_length(meta->dependencies) > 0)
	{
		LOG(LOG_INFO, "Dependencies");
		LOG(LOG_INFO, "==================================");
		next = meta->dependencies;
		while (next != NULL)
		{
			LOG(LOG_INFO, "%s", (gchar *)next->data);
			next = next->next;
		}
	}

	if (list_length(meta->cfgentries) > 0)
	{
		LOG(LOG_INFO, " ");
		LOG(LOG_INFO, "Configuration Params");
		LOG(LOG_INFO, "==================================");
		next = meta->cfgentries;
		while (next != NULL)
		{
			cfgentry_t * cfg = next->data;

			if (cfg->desc == NULL)
				LOG(LOG_INFO, "(%c) %s", cfg->type, cfg->name);
			else
				LOG(LOG_INFO, "(%c) %s - %s", cfg->type, cfg->name, cfg->desc);

			next = next->next;
		}
	}

	if (list_length(meta->calentries) > 0)
	{
		LOG(LOG_INFO, " ");
		LOG(LOG_INFO, "Calibration Params");
		LOG(LOG_INFO, "==================================");
		if (meta->calupdate != NULL)
			LOG(LOG_INFO, "Update: %s", meta->calupdate);
		next = meta->calentries;
		while (next != NULL)
		{
			calentry_t * cal = next->data;

			if (cal->desc == NULL)
				LOG(LOG_INFO, "(%s)\t%s", cal->sig, cal->name);
			else
				LOG(LOG_INFO, "(%s)\t%s - %s", cal->sig, cal->name, cal->desc);

			next = next->next;
		}
	}

	if (list_length(meta->syscalls) > 0)
	{
		LOG(LOG_INFO, " ");
		LOG(LOG_INFO, "Syscalls");
		LOG(LOG_INFO, "==================================");
		next = meta->syscalls;
		while (next != NULL)
		{
			syscall_t * syscall = next->data;

			if (syscall->desc == NULL)
				LOG(LOG_INFO, "%s(%s)", syscall->name, syscall->sig);
			else
				LOG(LOG_INFO, "%s(%s) - %s", syscall->name, syscall->sig, syscall->desc);

			next = next->next;
		}
	}

	if (g_hash_table_size(meta->blocks) > 0)
	{
		LOG(LOG_INFO, " ");
		LOG(LOG_INFO, "Blocks");
		LOG(LOG_INFO, "==================================");
		g_hash_table_iter_init(&itr, meta->blocks);
		while (g_hash_table_iter_next(&itr, NULL, (gpointer *)&block))
		{
			if (block->desc == NULL)
				LOG(LOG_INFO, "%s", block->name);
			else
				LOG(LOG_INFO, "%s - %s", block->name, block->desc);

			if (block->new_name != NULL)
				LOG(LOG_INFO, "  Constructor: %s(%s)", block->new_name, block->new_sig);

			if (block->ondestroy_name != NULL)
				LOG(LOG_INFO, "  Destructor: %s()", block->ondestroy_name);

			LOG(LOG_INFO, "  Update: %s", block->onupdate_name);

			next = block->inputs;
			while (next != NULL)
			{
				bio_t * in = next->data;

				if (in->desc == NULL)
					LOG(LOG_INFO, "    -> input  (%c) %s", in->sig, in->name);
				else
					LOG(LOG_INFO, "    -> input  (%c) %s - %s", in->sig, in->name, in->desc);

				next = next->next;
			}

			next = block->outputs;
			while (next != NULL)
			{
				bio_t * out = next->data;

				if (out->desc == NULL)
					LOG(LOG_INFO, "    <- output (%c) %s", out->sig, out->name);
				else
					LOG(LOG_INFO, "    <- output (%c) %s - %s", out->sig, out->name, out->desc);

				next = next->next;
			}
		}
	}

	LOG(LOG_INFO, " "); //empty line

	return 0;
}
