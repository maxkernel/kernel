#include <stdlib.h>
#include <string.h>

#include "kernel.h"
#include "kernel-priv.h"

extern GHashTable * modules;

cfgentry_t * cfg_getparam(const gchar * modname, const gchar * cfgname)
{
	module_t * module = module_get(modname);
	List * next = module->cfgentries;
	while (next != NULL)
	{
		cfgentry_t * cfg = next->data;
		if (strcmp(cfg->name, cfgname) == 0)
		{
			return cfg;
		}

		next = next->next;
	}

	return NULL;
}

void cfg_setparam(const gchar * modname, const gchar * cfgname, const gchar * value)
{
	module_t * module = module_get(modname);

	if (module == NULL)
	{
		LOGK(LOG_ERR, "Could not set parameter %s.%s, module %s does not exist!", modname, cfgname, modname);
		return;
	}

	cfgentry_t * cfg = cfg_getparam(modname, cfgname);
	if (cfg == NULL)
	{
		LOGK(LOG_ERR, "Could not set configuration parameter %s.%s, entry %s does not exist!", modname, cfgname, cfgname);
		return;
	}

	switch (cfg->type) {

		#define __cfg_setparam_element(t1, t2, t3) \
			case t1: { \
				LOGK(LOG_DEBUG, "Setting configuration parameter %s to value %s", cfg->name, value); \
				*(t2*)cfg->variable = t3; \
				break; }

		__cfg_setparam_element(T_BOOLEAN, gboolean, atoi(value))
		__cfg_setparam_element(T_INTEGER, gint, atoi(value))
		__cfg_setparam_element(T_DOUBLE, gdouble, g_strtod(value, NULL))
		__cfg_setparam_element(T_CHAR, gchar, value[0])
		__cfg_setparam_element(T_STRING, gchar*, g_strdup(value))

		default:
		{
			LOGK(LOG_ERR, "Unknown configuration type '%c' for %s", cfg->type, cfg->name);
		}
	}
}

