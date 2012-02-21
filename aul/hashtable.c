#include <string.h>

#include <aul/common.h>
#include <aul/log.h>
#include <aul/hashtable.h>


hashtable_t * hashtable_new(size_t numbuckets, hashcode_f hasher, hashequals_f equals)
{
	hashtable_t * table = malloc0( (sizeof(hashtable_t)-(sizeof(list_t)*AUL_HASHTABLE_BUCKETS)) + (sizeof(list_t)*numbuckets));

	table->hasher = hasher;
	table->equals = equals;
	table->numbuckets = numbuckets;

	LIST_INIT(&table->iterator);

	for (int i=0; i<numbuckets; i++)
	{
		LIST_INIT(&table->buckets[i]);
	}

	return table;
}


// Taken with permission (and slightly modified) from http://www.partow.net/programming/hashfunctions/
// (c) Arash Partow
static unsigned int DJBHash(const char * str, unsigned int len)
{
	unsigned int hash = 5381;
	unsigned int i    = 0;

	for(i = 0; i < len; i++)
	{
		hash = ((hash << 5) + hash) + str[i];
	}

	return hash;
}

// Hashcode functions
unsigned int hash_str(const void * key)
{
	const char * k = key;
	return DJBHash(k, strlen(k));
}

bool hash_streq(const void * a, const void * b)
{
	const char * aa = a, * bb = b;
	return strcmp(aa, bb) == 0;
}

unsigned int hash_int(const void * key)
{
	return (unsigned int)*(int *)key;
}

bool hash_inteq(const void * a, const void * b)
{
	return *(int *)a == *(int *)b;
}
