#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <aul/parse.h>

#include <kernel.h>
#include <kernel-priv.h>


static char * config_info(void * object)
{
	char * str = "[PLACEHOLDER CONFIG INFO]";
	return strdup(str);
}

void config_destroy(void * object)
{
	//config_t * config = object;
	// No custom destructor required
}

static bool config_updatecache(const meta_variable_t * variable, char * cachebacking, exception_t ** err)
{
	const char * name = NULL;
	char sig = 0;
	const void * value = NULL;
	meta_getvariable(variable, &name, &sig, NULL, &value);

	if (value == NULL)
	{
		exception_set(err, EINVAL, "Config parameter %s has no backing memory! Has module pointers been resolved?", name);
		return false;
	}

	size_t wrote = 0;
	char cache[CONFIG_SIZE_CACHE] = {0};

	switch (sig)
	{
		case T_BOOLEAN:
		{
			wrote = snprintf(cache, CONFIG_SIZE_CACHE, "%s", (*(const bool *)value)? "true" : "false");
			break;
		}

		case T_INTEGER:
		{

			wrote = snprintf(cache, CONFIG_SIZE_CACHE, "%d", *(const int *)value);
			break;
		}

		case T_DOUBLE:
		{
			wrote = snprintf(cache, CONFIG_SIZE_CACHE, "%f", *(const double *)value);
			break;
		}

		case T_CHAR:
		{
			wrote = snprintf(cache, CONFIG_SIZE_CACHE, "%c", *(const char *)value);
			break;
		}

		case T_STRING:
		{
			wrote = snprintf(cache, CONFIG_SIZE_CACHE, "%s", *(const char **)value);
			break;
		}

		default:
		{
			exception_set(err, EINVAL, "Unknown config type %s with sig %c", name, sig);
			return false;
		}
	}

	if (wrote >= (CONFIG_SIZE_CACHE - 1))
	{
		// Truncated!
		exception_set(err, ENOMEM, "Could not cache config entry %s too big! (CONFIG_SIZE_CACHE = %d)", name, CONFIG_SIZE_CACHE);
		return false;
	}

	strcpy(cachebacking, cache);
	return true;
}

static bool config_updatebacking(const meta_variable_t * variable, const char * newvalue, exception_t ** err)
{
	const char * name = NULL;
	char sig = 0;
	const void * value = NULL;
	meta_getvariable(variable, &name, &sig, NULL, &value);

	exception_t * e = NULL;
	switch (sig)
	{
		case T_BOOLEAN:
		{
			bool v = parse_bool(newvalue, &e);
			if (!exception_check(&e))	*(bool *)value = v;
			break;
		}
		case T_INTEGER:
		{
			int v = parse_int(newvalue, &e);
			if (!exception_check(&e))	*(int *)value = v;
			break;
		}

		case T_DOUBLE:
		{
			double v = parse_double(newvalue, &e);
			if (!exception_check(&e))	*(double *)value = v;
			break;
		}

		case T_CHAR:
		{
			*(char *)value = newvalue[0];
			break;
		}

		case T_STRING:
		{
			*(char **)value = strdup(newvalue);
			break;
		}

		default:
		{
			exception_set(&e, EINVAL, "Unknown config %s with sig %c", name, sig);
			break;
		}
	}

	if (exception_check(&e))
	{
		exception_set(err, e->code, "%s", e->message);
		exception_free(e);
		return false;
	}

	return true;
}

config_t * config_new(const meta_t * meta, const meta_variable_t * config_variable, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			// Exception already set
			return NULL;
		}

		if (meta == NULL || config_variable == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}

	const char * path = NULL;
	meta_getinfo(meta, &path, NULL, NULL, NULL, NULL);

	const char * name = NULL;
	char sig = 0;
	const void * value = NULL;
	meta_getvariable(config_variable, &name, &sig, NULL, &value);

	char cache[CONFIG_SIZE_CACHE] = {0};
	if (!config_updatecache(config_variable, cache, err))
	{
		// Error happened and exception_t set in updatecache, just return NULL
		return NULL;
	}

	LOGK(LOG_DEBUG, "Registered config variable %s in module %s", name, path);

	string_t objectname = string_new("%s -> %s", path, name);
	config_t * config = kobj_new("Config", objectname.string, config_info, config_destroy, sizeof(config_t));
	config->variable = config_variable;
	strcpy(config->cache, cache);

	return config;
}

bool config_apply(config_t * config, const model_config_t * config_newvalue, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			// Exception already set
			return false;
		}

		if (config == NULL || config_newvalue == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}

		if (config->variable == NULL)
		{
			exception_set(err, EINVAL, "Config variable backing is null!");
			return false;
		}
	}

	const char * name = NULL;
	char sig = 0;
	const void * value = NULL;
	meta_getvariable(config->variable, &name, &sig, NULL, &value);

	const char * newname = NULL;
	char newsig = 1;
	constraint_t constraints = constraint_empty();
	const char * newvalue = NULL;
	model_getconfig(config_newvalue, &newname, &newsig, &constraints, &newvalue);

	if (strlen(newvalue) >= CONFIG_SIZE_CACHE)
	{
		exception_set(err, ENOMEM, "Could not apply config %s, value too long! (CONFIG_SIZE_CACHE = %d)", name, CONFIG_SIZE_CACHE);
		return false;
	}

	if (strcmp(name, newname) != 0 || sig != newsig)
	{
		exception_set(err, EINVAL, "Applying config parameters invalid. Expected (%s, %c) got (%s, %c)", name, sig, newname, newsig);
		return false;
	}

	string_t hint = string_blank();
	if (!constraint_check(&constraints, newvalue, hint.string, string_available(&hint)))
	{
		exception_set(err, EINVAL, "Config %s, failed constraint check: %s", name, hint.string);
		return false;
	}

	LOGK(LOG_DEBUG, "Setting config variable %s='%s'", name, newvalue);

	if (!config_updatebacking(config->variable, newvalue, err))
	{
		// Error happened and exception_t set in updatebacking, just return false
		return false;
	}

	if (!config_updatecache(config->variable, config->cache, err))
	{
		// Error happened and exception_t set in updatecache, just return false
		return false;
	}

	return true;
}
