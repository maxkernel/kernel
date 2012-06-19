#include <string.h>

#include <serialize.h>
#include <max.h>


typedef struct
{
	hashtable_t syscalls;
	list_t syscalls_free;
	syscall_t syscalls_array[SYSCALL_CACHE_SIZE];

	mutex_t syscalls_mutex;
} syscall_cache_t;


void max_syscallcache_enable(maxhandle_t * hand)
{
	if (hand->syscall_cache != NULL)
	{
		// Cache already enabled
		return;
	}

	syscall_cache_t * cache = hand->syscall_cache = hand->malloc(sizeof(syscall_cache_t));
	if (hand->syscall_cache == NULL)
	{
		hand->memerr();
		return;
	}

	list_init(&cache->syscalls_free);
	hashtable_init(&cache->syscalls, hash_str, hash_streq);

	for (size_t i = 0; i < SYSCALL_CACHE_SIZE; i++)
	{
		list_add(&cache->syscalls_free, &cache->syscalls_array[i].syscalls_list);
	}

	mutex_init(&cache->syscalls_mutex, M_RECURSIVE);
}

void max_syscallcache_destroy(maxhandle_t * hand)
{
	if (hand->syscall_cache == NULL)
	{
		// Cache isn't enabled
		return;
	}

	hand->free(hand->syscall_cache);
	hand->syscall_cache = NULL;
}

syscall_t * max_syscallcache_lookup(maxhandle_t * hand, exception_t ** err, const char * name)
{
	syscall_cache_t * cache = hand->syscall_cache;
	if (cache == NULL)
	{
		return NULL;
	}

	mutex_lock(&cache->syscalls_mutex);
	{
		hashentry_t * entry = hashtable_get(&cache->syscalls, name);
		if (entry != NULL)
		{
			// Syscall already exists in cache, update last_access, and return it
			syscall_t * syscall = hashtable_entry(entry, syscall_t, syscalls_entry);
			syscall->last_access = time(NULL);

			return syscall;
		}
	}
	mutex_unlock(&cache->syscalls_mutex);

	return_t r;
	bool success = max_syscall(hand, err, &r, "syscall_signature", "s:s", name);
	if (exception_check(err) || !success || r.type != T_RETURN || strlen(r.data.t_string) == 0)
	{
		// Error'd on connection or syscall doesn't exist
		return NULL;
	}


	// Grab a free buffer, put syscall data into it, then add it to our cache
	syscall_t * syscall = NULL;
	mutex_lock(&cache->syscalls_mutex);
	{
		if (list_isempty(&cache->syscalls_free))
		{
			// No free syscall_t's, re-use one from cache. Find the oldest-accessed one
			list_t * pos = NULL;
			hashtable_foreach(pos, &cache->syscalls)
			{
				syscall_t * cmp = hashtable_itrentry(pos, syscall_t, syscalls_entry);
				if (syscall == NULL || cmp->last_access < syscall->last_access)
				{
					syscall = cmp;
				}
			}

			// Free the syscall cache entry
			hashtable_remove(&syscall->syscalls_entry);
		}
		else
		{
			// Get a syscall_t from the free list
			syscall = list_entry(cache->syscalls_free.next, syscall_t, syscalls_list);
			list_remove(&syscall->syscalls_list);
		}


		memset(syscall, 0, sizeof(syscall_t));
		strncpy(syscall->name, name, SYSCALL_CACHE_NAMELEN - 1);
		strncpy(syscall->sig, r.data.t_string, SYSCALL_CACHE_SIGLEN);
		syscall->last_access = time(NULL);

		hashtable_put(&cache->syscalls, syscall->name, &syscall->syscalls_entry);
	}
	mutex_unlock(&cache->syscalls_mutex);

	return syscall;
}

bool max_syscallcache_exists(maxhandle_t * hand, const char * name, const char * sig)
{
	syscall_cache_t * cache = hand->syscall_cache;
	if (cache == NULL)
	{
		return false;
	}

	syscall_t * syscall = max_syscallcache_lookup(hand, NULL, name);
	if (syscall != NULL && !method_isequal(syscall->sig, sig))
	{
		syscall = NULL;
	}

	return syscall != NULL;
}

const char * max_syscallcache_getsig(maxhandle_t * hand, const char * name)
{
	syscall_cache_t * cache = hand->syscall_cache;
	if (cache == NULL)
	{
		return NULL;
	}

	syscall_t * syscall = max_syscallcache_lookup(hand, NULL, name);

	return syscall == NULL? NULL : syscall->sig;
}
