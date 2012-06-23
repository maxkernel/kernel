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
			*(char **)&backing->data[0] = (char *)&backing->data[sizeof(char *)];
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
		case T_BOOLEAN:		*(bool *)iobacking_data(backing) = (data == NULL)? false : *(const bool *)data;			break;
		case T_INTEGER:		*(int *)iobacking_data(backing) = (data == NULL)? 0 : *(const int *)data;				break;
		case T_DOUBLE:		*(double *)iobacking_data(backing) = (data == NULL)? 0.0 : *(const double *)data;		break;
		case T_CHAR:		*(char *)iobacking_data(backing) = (data == NULL)? '\0' : *(const char *)data;			break;
		case T_STRING:
		{
			const char * str = (data == NULL)? "" : ((*(const char **)data == NULL)? "" : *(const char **)data);
			strncpy(*(char **)iobacking_data(backing), str, AUL_STRING_MAXLEN - 1);
			break;
		}

		case T_ARRAY_BOOLEAN:
		case T_ARRAY_INTEGER:
		case T_ARRAY_DOUBLE:
		case T_BUFFER:
		{
			if (iobacking_isnull(backing))
			{
				buffer_free(*(buffer_t **)iobacking_data(backing));
			}

			if (data != NULL)
			{
				*(buffer_t **)iobacking_data(backing) = buffer_dup(*(const buffer_t **)data);
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
