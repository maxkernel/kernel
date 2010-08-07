#include <string.h>

#include <aul/log.h>
#include <aul/hashtable.h>


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

static void hashtable_init(hashtable_t * ht)
{
	int i;

	// Initialize all the list pointers
	for (i=0; i<AUL_HASHTABLE_BUCKETS; i++)
	{
		INIT_LIST_HEAD(&ht->buckets[i]);
	}
	INIT_LIST_HEAD(&ht->itr);
	INIT_LIST_HEAD(&ht->empty);

	// Add the empty buckets to the 'empty' list
	for (i=0; i<AUL_HASHTABLE_ENTRIES; i++)
	{
		list_add(&ht->__list[i].bucket, &ht->empty);
	}
}

static hashentry_t * hashtable_getentry(hashtable_t * table, struct list_head * list, const void * key)
{
	struct list_head * pos;
	list_for_each(pos, list)
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
		if (list_empty(&table->empty))
		{
			log_write(LEVEL_ERROR, AUL_LOG_DOMAIN, "Out of free hashtable entries! Last value not added. Consider increasing limit (currently %d)", AUL_HASHTABLE_ENTRIES);
			return NULL;
		}
		
		// Entry doesn't exist, take one from the empty list
		hashentry_t * entry = list_entry(table->empty.next, hashentry_t, bucket);
		list_del(&entry->bucket);
		
		entry->key = key;
		entry->data = data;
		
		list_add(&entry->itr, &table->itr);
		list_add(&entry->bucket, &table->buckets[bucketnum]);
		
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

	list_del(&entry->bucket);
	list_del(&entry->itr);
	entry->key = NULL;
	entry->data = NULL;

	list_add(&entry->bucket, &table->empty);
	return data;
}

void hashtable_clear(hashtable_t * table)
{
	// Re-initialize the table (resets all lists)
	hashtable_init(table);
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

