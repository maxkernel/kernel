#include <stdlib.h>
#include <stdio.h>


#include <max.h>


typedef struct
{
	const char * name;
	const char * sig;
	const char * description;

	hashentry_t syscalls_entry;
} syscall_t;

#define check_malloc(handle, ptr) do { \
	if (ptr == NULL) { handle->memerr(); } \
while(0)


void max_init(maxhandle_t * hand)
{
	hand->error = NULL;
	hand->malloc = malloc;
	hand->free = free;
	hand->memerr = max_memerr;

	HASHTABLE_INIT(&hand->syscalls, hash_str, hash_streq);

	hand->sock = -1;
	mutex_init(&hand->sock_mutex, M_RECURSIVE);
}

void max_setmalloc(maxhandle_t * hand, malloc_f mfunc, free_f ffunc, memerr_f efunc)
{
	if (mfunc != NULL) hand->malloc = mfunc;
	if (ffunc != NULL) hand->free = ffunc;
	if (efunc != NULL) hand->memerr = efunc;
}

void max_memerr()
{
	fprintf(stderr, "Error: Out of memory\n");
	fflush(NULL);
	exit(EXIT_FAILURE);
}

bool max_connect(maxhandle_t * hand, const char * host)
{
	if (exception_check(hand->error))
	{
		// Exception already set
		return false;
	}

	if (hand->sock != -1)
	{
		// Socket already open
		hand->error = hand->malloc(sizeof(exception_t));
		check_malloc(hand, hand->error);


	}
}
