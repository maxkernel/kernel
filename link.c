#include <errno.h>

#include <aul/string.h>
#include <aul/exception.h>
#include <aul/mutex.h>

#include <maxmodel/meta.h>
#include <maxmodel/model.h>

#include <buffer.h>
#include <array.h>

#include <kernel.h>
#include <kernel-priv.h>


extern mutex_t io_lock;


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
		case T_ARRAY_DOUBLE:	return sizeof(array_t);
		case T_BUFFER:			return sizeof(buffer_t);
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
	backing->size = memsize;
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


static inline void link_handle(const linkfunc_t * link, const iobacking_t * from, iobacking_t * to)
{
	link->function(link, from, from->isnull, to->data, to->isnull);
	to->isnull = from->isnull;
}

void link_doinputs(portlist_t * ports, linklist_t * links)
{
	// Sanity check
	{
		if unlikely(ports == NULL || links == NULL)
		{
			LOGK(LOG_ERR, "Invalid parameters!");
			return;
		}
	}


	if (list_next(ports) == ports)
	{
		// Ports is an empty list (No ports to process)
		if (list_next(&links->inputs) != &links->inputs)
		{
			// There are links defined (to nonexistent ports!)
			LOGK(LOG_ERR, "Links defined to nonexistent ports!");
		}

		return;
	}

	mutex_lock(&io_lock);
	{
		list_t * pitem = list_next(ports);

		list_t * pos = NULL;
		list_foreach(pos, &links->inputs)
		{
			link_t * link = list_entry(pos, link_t, link_list);

			const char * symbol_name = NULL;
			model_getlinksymbol(link->symbol, NULL, &symbol_name, NULL, NULL);

			port_t * port = list_entry(pitem, port_t, port_list);
			while (!port_test(port, meta_input, symbol_name))
			{
				pitem = list_next(pitem);
				if (pitem == ports)
				{
					// Very, very baaad!
					LOGK(LOG_ERR, "Could not find input port symbol %s in portlist!", symbol_name);
					return;
				}

				port = list_entry(pitem, port_t, port_list);
			}

			link_handle(&link->link, link->backing, port->backing);
		}
	}
	mutex_unlock(&io_lock);
}

void link_dooutputs(portlist_t * ports, linklist_t * links)
{
	// Sanity check
	{
		if unlikely(ports == NULL || links == NULL)
		{
			LOGK(LOG_ERR, "Invalid parameters!");
			return;
		}
	}


	if (list_next(ports) == ports)
	{
		// Ports is an empty list (No ports to process)
		if (list_next(&links->outputs) != &links->outputs)
		{
			// There are links defined (to nonexistent ports!)
			LOGK(LOG_ERR, "Links defined to nonexistent ports!");
		}

		return;
	}

	mutex_lock(&io_lock);
	{
		list_t * pitem = list_next(ports);

		list_t * pos = NULL;
		list_foreach(pos, &links->outputs)
		{
			link_t * link = list_entry(pos, link_t, link_list);

			const char * symbol_name = NULL;
			model_getlinksymbol(link->symbol, NULL, &symbol_name, NULL, NULL);

			port_t * port = list_entry(pitem, port_t, port_list);
			while (!port_test(port, meta_output, symbol_name))
			{
				pitem = list_next(pitem);
				if (pitem == ports)
				{
					// Very, very baaad!
					LOGK(LOG_ERR, "Could not find input port symbol %s in portlist!", symbol_name);
					return;
				}

				port = list_entry(pitem, port_t, port_list);
			}

			link_handle(&link->link, port->backing, link->backing);
		}
	}
	mutex_unlock(&io_lock);
}



static void copy_direct(const linkfunc_t * link, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	memcpy(to, from, link->data.length);
}

static void copy_string(const linkfunc_t * link, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	strncpy(*(char **)to, *(const char **)from, AUL_STRING_MAXLEN - 1);
}

static void copy_buffer(const linkfunc_t * link, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	if (!to_isnull)		buffer_free(*(buffer_t *)to);
	if (!from_isnull)	*(buffer_t *)to = buffer_dup(*(const buffer_t *)from);
}

// TODO - make two functions, one for copy out, the other for copy in??
bool link_getfunction(const model_link_t * model_link, char from_sig, char to_sig, linkfunc_t * link)
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
			link->data.length = iobacking_size(to_sig);
			return true;
		}

		case T_INTEGER:
		{
			switch (to_sig)
			{
				case T_INTEGER:
				{
					link->function = copy_direct;
					link->data.length = iobacking_size(to_sig);
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
					link->data.length = iobacking_size(to_sig);
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
			return true;
		}

		default:
		{
			return false;
		}
	}
}
