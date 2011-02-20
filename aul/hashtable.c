#include <string.h>

#include <aul/common.h>
#include <aul/log.h>
#include <aul/hashtable.h>

#if 0
static void hashtable_init(hashtable_t * ht)
{
	int i;

	// Initialize all the list pointers
	for (i=0; i<AUL_HASHTABLE_BUCKETS; i++)
	{
		LIST_INIT(&ht->buckets[i]);
	}
	LIST_INIT(&ht->itr);
	LIST_INIT(&ht->empty);

	// Add the empty buckets to the 'empty' list
	for (i=0; i<AUL_HASHTABLE_ENTRIES; i++)
	{
		list_add(&ht->empty, &ht->__list[i].bucket);
	}
}

static hashentry_t * hashtable_getentry(hashtable_t * table, list_t * list, const void * key)
{
	list_t * pos;
	list_foreach(pos, list)
	{
		hashentry_t * item = list_entry(pos, hashentry_t, bucket);
		if (table->equals(key, item->key))
		{
			return item;
		}
	}

	return NULL;
}

hashtable_t hashtable_new(hashcode_f hasher, hashequals_f equals)
{
	hashtable_t ht;
	
	ZERO(ht);
	ht.hasher = hasher;
	ht.equals = equals;
	
	hashtable_init(&ht);
	
	return ht;
}

void * hashtable_put(hashtable_t * table, const void * key, const void * data)
{
	unsigned int bucketnum = table->hasher(key) % AUL_HASHTABLE_BUCKETS;
	hashentry_t * entry = hashtable_getentry(table, &table->buckets[bucketnum], key);
	
	if (entry == NULL)
	{
		if (list_isempty(&table->empty))
		{
			log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Out of free hashtable entries! Last value not added. Consider increasing limit (currently %d)", AUL_HASHTABLE_ENTRIES);
			return NULL;
		}
		
		// Entry doesn't exist, take one from the empty list
		hashentry_t * entry = list_entry(table->empty.next, hashentry_t, bucket);
		list_remove(&entry->bucket);
		
		entry->key = key;
		entry->data = data;
		
		list_add(&table->itr, &entry->itr);
		list_add(&table->buckets[bucketnum], &entry->bucket);
		
		return NULL;
	}
	else
	{
		// Entry already exists, overwrite with new data and return old
		
		void * olddata = (void *) entry->data;
		entry->data = data;
		
		return olddata;
	}
}

void * hashtable_get(hashtable_t * table, const void * key)
{
	unsigned int bucketnum = table->hasher(key) % AUL_HASHTABLE_BUCKETS;
	hashentry_t * entry = hashtable_getentry(table, &table->buckets[bucketnum], key);

	if (entry == NULL)
	{
		return NULL;
	}

	return (void *)entry->data;
}

void * hashtable_remove(hashtable_t * table, const void * key)
{
	unsigned int bucketnum = table->hasher(key) % AUL_HASHTABLE_BUCKETS;
	hashentry_t * entry = hashtable_getentry(table, &table->buckets[bucketnum], key);

	if (entry == NULL)
	{
		return NULL;
	}

	void * data = (void *)entry->data;

	list_remove(&entry->bucket);
	list_remove(&entry->itr);
	entry->key = NULL;
	entry->data = NULL;

	list_add(&table->empty, &entry->bucket);
	return data;
}

void hashtable_clear(hashtable_t * table)
{
	// Re-initialize the table (resets all lists)
	hashtable_init(table);
}
#endif


hashtable_t * hashtable_new(size_t numbuckets, hashcode_f hasher, hashequals_f equals)
{
	hashtable_t * table = malloc0( (sizeof(hashtable_t)-(sizeof(list_t)*AUL_HASHTABLE_BUCKETS)) + (sizeof(list_t)*numbuckets));

	table->hasher = hasher;
	table->equals = equals;
	table->numbuckets = numbuckets;

	LIST_INIT(&table->iterator);

	int i=0;
	for (; i<numbuckets; i++)
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
