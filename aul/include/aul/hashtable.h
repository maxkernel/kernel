#ifndef __HASHTABLE_H
#define __HASHTABLE_H

#include <aul/common.h>
#include <aul/list.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUL_HASHTABLE_BUCKETS		20


typedef unsigned int (*hashcode_f)(const void * key);
typedef bool (*hashequals_f)(const void * a, const void * b);

typedef struct
{
	const void * key;
	list_t itr;
	list_t bucket;
} hashentry_t;

typedef struct
{
	hashcode_f hasher;
	hashequals_f equals;
	size_t numbuckets;
	list_t iterator;
	list_t buckets[AUL_HASHTABLE_BUCKETS];
} hashtable_t;

#define HASHTABLE_INIT(ptr, hashfunc, equalsfunc) do { \
	(ptr)->hasher = (hashfunc); (ptr)->equals = (equalsfunc); \
	(ptr)->numbuckets = AUL_HASHTABLE_BUCKETS; \
	LIST_INIT(&(ptr)->iterator); \
	for (int i=0; i<AUL_HASHTABLE_BUCKETS; ++i) { LIST_INIT(&(ptr)->buckets[i]); } \
} while (0)


static inline void hashtable_remove(hashentry_t * entry)
{
	// Sanity check
	if (entry == NULL)
	{
		return;
	}

	list_remove(&entry->itr);
	list_remove(&entry->bucket);
	entry->key = NULL;
}

static hashentry_t * __hashtable_get(hashtable_t * table, list_t * list, const void * key)
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

static inline hashentry_t * hashtable_get(hashtable_t * table, const void * key)
{
	size_t bucketnum = table->hasher(key) % table->numbuckets;
	return __hashtable_get(table, &table->buckets[bucketnum], key);
}

static inline hashentry_t * hashtable_put(hashtable_t * table, const void * key, hashentry_t * entry)
{
	size_t bucketnum = table->hasher(key) % table->numbuckets;
	hashentry_t * oldentry = __hashtable_get(table, &table->buckets[bucketnum], key);

	if (oldentry != NULL)
	{
		hashtable_remove(oldentry);
	}

	entry->key = key;
	list_add(&table->iterator, &entry->itr);
	list_add(&table->buckets[bucketnum], &entry->bucket);

	return oldentry;
}

static inline bool hashtable_isempty(hashtable_t * table)
{
	return list_isempty(&table->iterator);
}

static inline size_t hashtable_size(hashtable_t * table)
{
	return list_size(&table->iterator);
}

static inline list_t * hashtable_itr(hashtable_t * table)
{
	return &table->iterator;
}

#define hashtable_entry(ptr, type, member) \
	list_entry((ptr), type, member)

#define hashtable_itrentry(ptr, type, member) \
	list_entry(list_entry((ptr), hashentry_t, itr), type, member)

/**
 * hashtable_foreach	-	iterate over a hashtable in the order elements were added
 * @pos:	the &list_t to use as a loop counter.
 * @table:	the hashtable.
 */
#define hashtable_foreach(pos, table) \
		for ((pos) = (table)->iterator.next; \
		(pos) != &(table)->iterator; \
		(pos) = (pos)->next)

/**
 * hashtable_foreach_safe	-	iterate over a hashtable safe against removal of entry
 * @pos:	the &list_t to use as a loop counter.
 * @n:		another &list_t to use as temporary storage
 * @table:	the hashtable.
 */
#define hashtable_foreach_safe(pos, n, table) \
	for ((pos) = (table)->iterator.next, (n) = (pos)->next; \
	(pos) != &(table)->iterator; \
	(pos) = (n), (n) = (pos)->next)


// .c functions
hashtable_t * hashtable_new(size_t numbuckets, hashcode_f hasher, hashequals_f equals);

// Hashcode functions
unsigned int hash_str(const void * key);
bool hash_streq(const void * a, const void * b);
unsigned int hash_int(const void * key);
bool hash_inteq(const void * a, const void * b);

#ifdef __cplusplus
}
#endif
#endif
