#include <stdlib.h>
#include <string.h>

#include <kernel.h>
#include <kernel-priv.h>

const cfgentry_t * cfg_getparam(const char * modname, const char * cfgname)
{
	const list_t * pos;
	const module_t * module = module_lookup(modname);

	list_foreach(pos, &module->cfgentries)
	{
		const cfgentry_t * cfg = list_entry(pos, cfgentry_t, module_list);
		if (strcmp(cfgname, cfg->name) == 0)
		{
			return cfg;
		}
	}

	return NULL;
}

void cfg_setparam(const char * modname, const char * cfgname, const char * value)
{
	LABELS(err_parse);

	const module_t * module = module_lookup(modname);

	if (module == NULL)
	{
		LOGK(LOG_ERR, "Could not set parameter %s.%s, module %s does not exist!", modname, cfgname, modname);
		return;
	}

	const cfgentry_t * cfg = cfg_getparam(modname, cfgname);
	if (cfg == NULL)
	{
		LOGK(LOG_ERR, "Could not set configuration parameter %s.%s, entry %s does not exist!", modname, cfgname, cfgname);
		return;
	}

	LOGK(LOG_DEBUG, "Setting configuration parameter %s to value %s", cfg->name, value);

	exception_t * e = NULL;
	switch (cfg->type) {

		case T_BOOLEAN:
		{
			bool v = parse_bool(value, &e);
			if (exception_check(&e))
			{
				goto err_parse;
			}

			*(bool *)cfg->variable = v;
			break;
		}

		case T_INTEGER:
		{
			int v = parse_int(value, &e);
			if (exception_check(&e))
			{
				goto err_parse;
			}

			*(int *)cfg->variable = v;
			break;
		}

		case T_DOUBLE:
		{
			double v = parse_double(value, &e);
			if (exception_check(&e))
			{
				goto err_parse;
			}

			*(double *)cfg->variable = v;
			break;
		}

		case T_CHAR:
		{
			*(char *)cfg->variable = value[0];
			break;
		}

		case T_STRING:
		{
			*(char **)cfg->variable = strdup(value);
			break;
		}

		default:
		{
			LOGK(LOG_ERR, "Unknown configuration type '%c' for %s", cfg->type, cfg->name);
			break;
		}
	}

	return;

err_parse:
	LOGK(LOG_ERR, "Could not set config value %s: %s", cfg->name, e->message);
	return;
}

