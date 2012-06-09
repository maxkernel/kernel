#include <aul/hashtable.h>
#include <kernel.h>
#include <kernel-priv.h>

extern hashtable_t properties;

void property_set(const char * name, const char * value)
{
	hashentry_t * entry = hashtable_get(&properties, name);
	if (entry == NULL)
	{
		property_t * prop = malloc(sizeof(property_t));
		memset(prop, 0, sizeof(property_t));
		prop->name = strdup(name);
		prop->value = strdup(value);
		hashtable_put(&properties, prop->name, &prop->entry);
	}
	else
	{
		property_t * prop = hashtable_entry(entry, property_t, entry);
		free(prop->value);
		prop->value = strdup(value);
	}
}

void property_clear(const char * name)
{
	hashentry_t * entry = hashtable_get(&properties, name);
	if (entry != NULL)
	{
		hashtable_remove(entry);

		property_t * prop = hashtable_entry(entry, property_t, entry);
		free(prop->name);
		free(prop->value);
		free(prop);
	}
}

const char * property_get(const char * name)
{
	hashentry_t * entry = hashtable_get(&properties, name);
	if (entry == NULL)
	{
		return NULL;
	}

	return hashtable_entry(entry, property_t, entry)->value;
}

bool property_isset(const char * name)
{
	return hashtable_get(&properties, name) != NULL;
}
