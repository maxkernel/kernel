#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <aul/parse.h>

#include <kernel.h>
#include <kernel-priv.h>


static ssize_t config_info(kobject_t * object, char * buffer, size_t length)
{
	config_t * config = (config_t *)object;
	return snprintf(buffer, length, "{ name: %s, signature: %c, value: '%s', description: '%s' }", config->name, config->sig, ser_string(config->cache), ser_string(config->desc));
}

void config_destroy(kobject_t * object)
{
	config_t * config = (config_t *)object;
	free(config->name);
}

static bool config_updatecache(const meta_variable_t * variable, char * cachebacking, exception_t ** err)
{
	const char * name = NULL;
	char sig = 0;
	void * value = NULL;
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
			wrote = snprintf(cache, CONFIG_SIZE_CACHE, "%s", (*(bool *)value)? "true" : "false");
			break;
		}

		case T_INTEGER:
		{

			wrote = snprintf(cache, CONFIG_SIZE_CACHE, "%d", *(int *)value);
			break;
		}

		case T_DOUBLE:
		{
			wrote = snprintf(cache, CONFIG_SIZE_CACHE, "%f", *(double *)value);
			break;
		}

		case T_CHAR:
		{
			wrote = snprintf(cache, CONFIG_SIZE_CACHE, "%c", *(char *)value);
			break;
		}

		case T_STRING:
		{
			wrote = snprintf(cache, CONFIG_SIZE_CACHE, "%s", *(char **)value);
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
	void * value = NULL;
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

config_t * config_new(const meta_t * meta, const meta_variable_t * config, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(meta == NULL || config == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}

	const char * name = NULL, * desc = NULL;
	char sig = 0;
	meta_getvariable(config, &name, &sig, &desc, NULL);

	LOGK(LOG_DEBUG, "Registering config variable '%s'", name);

	char cache[CONFIG_SIZE_CACHE] = {0};
	if (!config_updatecache(config, cache, err))
	{
		return NULL;
	}

	config_t * c = kobj_new("Config", name, config_info, config_destroy, sizeof(config_t));
	c->name = strdup(name);
	c->sig = sig;
	c->desc = strdup(desc);
	c->variable = config;
	strcpy(c->cache, cache);

	return c;
}

bool config_apply(config_t * config, const model_config_t * newvalue, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(config == NULL || newvalue == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}

		if unlikely(config->variable == NULL)
		{
			exception_set(err, EINVAL, "Config variable backing is null!");
			return false;
		}
	}

	const char * old_name = NULL;
	char old_sig = 0;
	meta_getvariable(config->variable, &old_name, &old_sig, NULL, NULL);

	const char * new_name = NULL;
	char new_sig = 1;
	constraint_t constraints = constraint_empty();
	const char * new_value = NULL;
	model_getconfig(newvalue, &new_name, &new_sig, &constraints, &new_value);

	if (strlen(new_value) >= CONFIG_SIZE_CACHE)
	{
		exception_set(err, ENOMEM, "Could not apply config %s, value too long! (CONFIG_SIZE_CACHE = %d)", old_name, CONFIG_SIZE_CACHE);
		return false;
	}

	if (strcmp(old_name, new_name) != 0 || old_sig != new_sig)
	{
		exception_set(err, EINVAL, "Applying config parameters invalid. Expected (%s, %c) got (%s, %c)", old_name, old_sig, new_name, new_sig);
		return false;
	}

	string_t hint = string_blank();
	if (!constraint_check(&constraints, new_value, hint.string, string_available(&hint)))
	{
		exception_set(err, EINVAL, "Config %s, failed constraint check: %s", old_name, hint.string);
		return false;
	}

	LOGK(LOG_DEBUG, "Setting config variable %s to '%s'", old_name, new_value);

	if (!config_updatebacking(config->variable, new_value, err))
	{
		return false;
	}

	if (!config_updatecache(config->variable, config->cache, err))
	{
		return false;
	}

	return true;
}
