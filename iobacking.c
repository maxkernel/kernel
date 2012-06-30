#include <errno.h>

#include <aul/common.h>
#include <aul/exception.h>

#include <array.h>
#include <buffer.h>
#include <kernel.h>
#include <kernel-priv.h>

static size_t iobacking_size(char sig)
{
	switch (sig)
	{
		case T_VOID:			return 0;
		case T_BOOLEAN:			return sizeof(bool);
		case T_INTEGER:			return sizeof(int);
		case T_DOUBLE:			return sizeof(double);
		case T_CHAR:			return sizeof(char);
		case T_STRING:			return sizeof(char *) + sizeof(char) * AUL_STRING_MAXLEN;
		case T_ARRAY_BOOLEAN:
		case T_ARRAY_INTEGER:
		case T_ARRAY_DOUBLE:	return sizeof(array_t *);
		case T_BUFFER:			return sizeof(buffer_t *);
		default:				return -1;
	}
}

iobacking_t * iobacking_new(char sig, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}
	}

	// Get the payload size
	ssize_t memsize = iobacking_size(sig);
	if (memsize == -1)
	{
		exception_set(err, EINVAL, "Could not determine backing size for sig '%c'", sig);
		return NULL;
	}

	iobacking_t * backing = malloc(sizeof(iobacking_t) + memsize);
	memset(backing, 0, sizeof(iobacking_t) + memsize);
	backing->sig = sig;
	backing->isnull = true;


	// Handle type-specific initialization
	switch (sig)
	{
		case T_STRING:
		{
			// Set up the buffer so that it looks like this:
			//  [  char *  |   string...      ]
			// First member points to the rest of the string
			//   we use this so that when we return the data chunk pointer, its of type pointer to char *
			char ** head = (char **)&backing[0];
			*head = (char *)&backing->data[sizeof(char *)];
			break;
		}

		default: break;
	}


	return backing;
}

void iobacking_destroy(iobacking_t * backing)
{
	// Sanity check
	{
		if unlikely(backing == NULL)
		{
			return;
		}
	}

	free(backing);
}

void iobacking_copy(iobacking_t * backing, const void * data)
{
	// Sanity check
	{
		if unlikely(backing == NULL)
		{
			return;
		}
	}

	// Copy data into backing
	switch (iobacking_sig(backing))
	{
		case T_BOOLEAN:
		{
			const bool * from = (const bool *)data;
			bool * to = (bool *)iobacking_data(backing);
			*to = (from == NULL)? false : *from;
			break;
		}

		case T_INTEGER:
		{
			const int * from = (const int *)data;
			int * to = (int *)iobacking_data(backing);
			*to = (from == NULL)? 0 : *from;
			break;
		}

		case T_DOUBLE:
		{
			const double * from = (const double *)data;
			double * to = (double *)iobacking_data(backing);
			*to = (from == NULL)? 0.0 : *from;
			break;
		}

		case T_CHAR:
		{
			const char * from = (const char *)data;
			char * to = (char *)iobacking_data(backing);
			*to = (from == NULL)? '\0' : *from;
			break;
		}

		case T_STRING:
		{
			const char * const * from = (const char * const *)data;
			char ** to = (char **)iobacking_data(backing);
			const char * str = (from == NULL)? "" : ((*from == NULL)? "" : *from);
			strncpy(*to, str, AUL_STRING_MAXLEN - 1);
			break;
		}

		case T_ARRAY_BOOLEAN:
		case T_ARRAY_INTEGER:
		case T_ARRAY_DOUBLE:
		case T_BUFFER:
		{
			const buffer_t ** from = (const buffer_t **)data;
			buffer_t ** to = (buffer_t **)iobacking_data(backing);

			if (!iobacking_isnull(backing))
			{
				buffer_free(*to);
				*to = NULL;
			}

			if (from != NULL)
			{
				*to = buffer_dup(*from);
			}

			break;
		}

		/*
		default:
		{
			LOGK(LOG_ERR, "Unknown signature backing: %c", iobacking_sig(backing));
			break;
		}
		*/
	}

	iobacking_isnull(backing) = (data == NULL);
}
