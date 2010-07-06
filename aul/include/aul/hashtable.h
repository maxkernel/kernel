#ifndef __HASHTABLE_H
#define __HASHTABLE_H

#include <aul/common.h>
#include <aul/contrib/list.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUL_HASHTABLE_ENTRIES		100
#define AUL_HASHTABLE_BUCKETS		20


typedef unsigned int (*hashcode_f)(const void * key);
typedef boolean (*hashequals_f)(const void * a, const void * b);

typedef struct
{
	const void * key;
	const void * data;
	struct list_head bucket;
	struct list_head itr;
} hashentry_t;

typedef struct
{
	hashcode_f hasher;
	hashequals_f equals;
	struct list_head buckets[AUL_HASHTABLE_BUCKETS];
	struct list_head itr;
	struct list_head empty;
	
	hashentry_t __list[AUL_HASHTABLE_ENTRIES];
} hashtable_t;


hashtable_t hashtable_new(hashcode_f hasher, hashequals_f equals);
void * hashtable_put(hashtable_t * table, const void * key, const void * data);
void * hashtable_get(hashtable_t * table, const void * key);
void * hashtable_remove(hashtable_t * table, const void * key);
void hashtable_clear(hashtable_t * table);


// Hashcode functions
unsigned int hash_str(const void * key);
boolean hash_streq(const void * a, const void * b);

#ifdef __cplusplus
}
#endif
#endif
