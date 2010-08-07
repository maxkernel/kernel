#include "kernel.h"

extern GHashTable * properties;

void property_set(char * name, char * value)
{
	if (property_isset(name))
	{
		g_free(property_get(name));
	}

	g_hash_table_insert(properties, name, g_strdup(value));
}

void property_clear(const char * name)
{
	char * prop = property_get(name);
	g_hash_table_remove(properties, name);
	g_free(prop);
}

char * property_get(const char * name)
{
	return g_hash_table_lookup(properties, name);
}

int property_get_i(const char * name)
{
	return atoi(property_get(name));
}

double property_get_d(const char * name)
{
	return g_strtod(property_get(name), NULL);
}

bool property_isset(const char * name)
{
	return property_get(name) != NULL;
}
