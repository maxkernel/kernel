#include <errno.h>

#include <aul/string.h>
#include <aul/exception.h>

#include <maxmodel/meta.h>
#include <maxmodel/model.h>

#include <buffer.h>
#include <array.h>

#include <kernel.h>
#include <kernel-priv.h>


static size_t link_backingsize(char sig)
{
	switch (sig)
	{
		case T_VOID:			return 0;
		case T_BOOLEAN:			return sizeof(bool);
		case T_INTEGER:			return sizeof(int);
		case T_DOUBLE:			return sizeof(double);
		case T_CHAR:			return sizeof(char);
		case T_STRING:			return sizeof(char) * AUL_STRING_MAXLEN;
		case T_ARRAY_BOOLEAN:
		case T_ARRAY_INTEGER:
		case T_ARRAY_DOUBLE:	return sizeof(array_t);
		case T_BUFFER:			return sizeof(buffer_t);
		default:				return -1;
	}
}

biobacking_t * link_newbacking(char sig, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}
	}

	// Get the payload size
	ssize_t memsize = link_backingsize(sig);
	if (memsize == -1)
	{
		exception_set(err, EINVAL, "Could not determine backing size for sig '%c'", sig);
		return NULL;
	}

	biobacking_t * backing = malloc(sizeof(biobacking_t) + memsize);
	memset(backing, 0, sizeof(biobacking_t) + memsize);
	backing->size = memsize;
	backing->sig = sig;
	backing->isnull = true;

	return backing;
}

void link_freebacking(biobacking_t * backing)
{
	// Sanity check
	{
		if (backing == NULL)
		{
			return;
		}
	}

	free(backing);
}



static void copy_direct(const link_t * link, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	memcpy(to, from, link->data.length);
}

static void copy_string(const link_t * link, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	strncpy((char *)to, (char *)from, link->data.length);
	((char *)to)[link->data.length - 1] = '\0';
}

static void copy_buffer(const link_t * link, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	if (!to_isnull)		buffer_free(*(buffer_t *)to);
	if (!from_isnull)	*(buffer_t *)to = buffer_dup(*(const buffer_t *)from);
}

// TODO - make two functions, one for copy out, the other for copy in??
bool link_getfunction(const model_link_t * model_link, char from_sig, char to_sig, link_t * link)
{
	switch (from_sig)
	{
		case T_BOOLEAN:
		case T_CHAR:
		{
			if (from_sig != to_sig)
			{
				return false;
			}

			link->function = copy_direct;
			link->data.length = link_backingsize(to_sig);
			return true;
		}

		case T_INTEGER:
		{
			switch (to_sig)
			{
				case T_INTEGER:
				{
					link->function = copy_direct;
					link->data.length = link_backingsize(to_sig);
					return true;
				}

				default:
				{
					return false;
				}
			}
		}

		case T_DOUBLE:
		{
			switch (to_sig)
			{
				case T_DOUBLE:
				{
					link->function = copy_direct;
					link->data.length = link_backingsize(to_sig);
					return true;
				}

				default:
				{
					return false;
				}
			}
		}

		case T_STRING:
		{
			if (from_sig != to_sig)
			{
				return false;
			}

			link->function = copy_string;
			link->data.length = link_backingsize(to_sig);
			return true;
		}

		default:
		{
			return false;
		}
	}
}