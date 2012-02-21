

#include <aul/common.h>
#include <aul/list.h>
#include <aul/mutex.h>
#include <aul/iterator.h>


static struct
{
	itrnext_f next_f;
	itrfree_f free_f;
	void * data;
} iterators[AUL_ITERATOR_MAX_ITRS];

static mutex_t iterators_mutex;


void iterator_init()
{
	memset(iterators, 0, sizeof(iterators));
	mutex_init(&iterators_mutex, M_RECURSIVE);
}

iterator_t iterator_new(itrnext_f next_func, itrfree_f free_func, void * object)
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
			iterators[itr].next_f = next_func;
			iterators[itr].free_f = free_func;
			iterators[itr].data = object;
		}
	}
	mutex_unlock(&iterators_mutex);

	return itr;
}

const void * iterator_next(iterator_t itr)
{
	// Sanity check
	if (iterators[itr].next_f == NULL || itr < 0 || itr >= AUL_ITERATOR_MAX_ITRS)
	{
		return NULL;
	}

	return iterators[itr].next_f(&iterators[itr].data);
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
		iterators[itr].free_f(iterators[itr].data);
	}

	mutex_lock(&iterators_mutex);
	{
		iterators[itr].next_f = NULL;
		iterators[itr].free_f = NULL;
		iterators[itr].data = NULL;
	}
	mutex_unlock(&iterators_mutex);
}
