

#include <aul/common.h>
#include <aul/list.h>
#include <aul/mutex.h>
#include <aul/iterator.h>


static struct
{
	const char * class;
	itrnext_f next_f;
	itrfree_f free_f;
	const void * object;
	void * itrobject;
} iterators[AUL_ITERATOR_MAX_ITRS];

static mutex_t iterators_mutex;


void iterator_init()
{
	memset(iterators, 0, sizeof(iterators));
	mutex_init(&iterators_mutex, M_RECURSIVE);
}

iterator_t iterator_new(const char * class, itrnext_f next_func, itrfree_f free_func, const void * object, void * itrobject)
{
	iterator_t itr = -1;

	// Sanity check
	if (next_func == NULL)
	{
		return -1;
	}

	mutex_lock(&iterators_mutex);
	{
		for (size_t index = 0; index < AUL_ITERATOR_MAX_ITRS; index++)
		{
			if (iterators[index].next_f == NULL)
			{
				itr = index;
				break;
			}
		}

		if (itr != -1)
		{
			iterators[itr].class = class;
			iterators[itr].next_f = next_func;
			iterators[itr].free_f = free_func;
			iterators[itr].object = object;
			iterators[itr].itrobject = itrobject;
		}
	}
	mutex_unlock(&iterators_mutex);

	return itr;
}

const void * iterator_next(iterator_t itr, const char * class)
{
	// Sanity check
	if (iterators[itr].next_f == NULL || itr < 0 || itr >= AUL_ITERATOR_MAX_ITRS)
	{
		return NULL;
	}

	// Check class
	if (class != NULL && strcmp(class, iterators[itr].class) != 0)
	{
		return NULL;
	}

	return iterators[itr].next_f(iterators[itr].object, &iterators[itr].itrobject);
}

void iterator_free(iterator_t itr)
{
	// Sanity check
	if (itr < 0 || itr >= AUL_ITERATOR_MAX_ITRS)
	{
		// Invalid iterator!
		return;
	}

	if (iterators[itr].free_f != NULL)
	{
		iterators[itr].free_f(iterators[itr].object, iterators[itr].itrobject);
	}

	mutex_lock(&iterators_mutex);
	{
		iterators[itr].next_f = NULL;
		iterators[itr].free_f = NULL;
		iterators[itr].object = NULL;
		iterators[itr].itrobject = NULL;
	}
	mutex_unlock(&iterators_mutex);
}
