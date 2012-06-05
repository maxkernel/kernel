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

static inline void link_handle(const link_f function, const void * data, const iobacking_t * from, iobacking_t * to)
{
	function(data, from, from->isnull, to->data, to->isnull);
	to->isnull = from->isnull;
}

iobacking_t * link_connect(const model_link_t * link, char outsig, linklist_t * outlinks, char insig, linklist_t * inlinks, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(link == NULL || outlinks == NULL || inlinks == NULL)
		{
			exception_set(err, EINVAL, "Invalid arguments!");
			return false;
		}
	}

	// Get the symbol information for both sides of the connection
	const model_linksymbol_t * outsym = NULL, * insym = NULL;
	model_getlink(link, &outsym, &insym);
	if (outsym == NULL || insym == NULL)
	{
		exception_set(err, EINVAL, "Malformed link (null terminal)");
		return false;
	}

	// Determine the intermediary data type
	char sig = '\0';
	switch (outsig)
	{
		case T_BOOLEAN:
		{
			switch (insig)
			{
				case T_BOOLEAN:			sig = T_BOOLEAN;		break;
				default:										break;
			}
		}

		case T_INTEGER:
		{
			switch (insig)
			{
				case T_INTEGER:			sig = T_INTEGER;		break;
				case T_DOUBLE:			sig = T_DOUBLE;			break;
				default:										break;
			}
		}

		case T_DOUBLE:
		{
			switch (insig)
			{
				case T_DOUBLE:			sig = T_DOUBLE;			break;
				case T_INTEGER:			sig = T_INTEGER;		break;
				default:										break;
			}
		}

		case T_CHAR:
		{
			switch (insig)
			{
				case T_CHAR:			sig = T_CHAR;			break;
				default:										break;
			}
		}

		case T_STRING:
		{
			switch (insig)
			{
				case T_STRING:			sig = T_STRING;			break;
				default:										break;
			}
		}

		case T_ARRAY_BOOLEAN:
		{
			/*
			switch (insig)
			{
				case T_ARRAY_BOOLEAN:	return T_ARRAY_BOOLEAN;
				default:				return '\0';
			}
			*/
			break;
		}

		case T_ARRAY_INTEGER:
		{
			/*
			switch (insig)
			{
				case T_ARRAY_INTEGER:	return T_ARRAY_INTEGER;
				default:				return '\0';
			}
			*/
			break;
		}

		case T_ARRAY_DOUBLE:
		{
			/*
			switch (insig)
			{
				case T_ARRAY_DOUBLE:	return T_ARRAY_DOUBLE;
				default:				return '\0';
			}
			*/
			break;
		}

		case T_BUFFER:
		{
			switch (insig)
			{
				case T_BUFFER:			sig = T_BUFFER;			break;
				default:										break;
			}
		}

		default: break;
	}
	if (sig == '\0')
	{
		exception_set(err, EINVAL, "Could not determine a proper link solution for %c -> %c", outsig, insig);
		return false;
	}

	// Create the intermediary backing
	iobacking_t * backing = iobacking_new(sig, err);
	if (backing == NULL || exception_check(err))
	{
		return false;
	}

	// Create both links, first from out -> interm, then from interm -> in
	link_t * outlink = malloc(sizeof(link_t));
	memset(outlink, 0, sizeof(link_t));
	outlink->symbol = outsym;
	outlink->backing = backing;
	outlink->linkfunction = link_getfunction(outsym, outsig, sig, &outlink->linkdata);

	link_t * inlink = malloc(sizeof(link_t));
	memset(inlink, 0, sizeof(link_t));
	inlink->symbol = insym;
	inlink->backing = backing;
	inlink->linkfunction = link_getfunction(insym, sig, insig, &inlink->linkdata);

	// If we could not find the link functions for both out -> interm and interm -> in, fail
	// TODO - Possible memory leak, commented out check because fuck it.
	if (outlink->linkfunction == NULL || inlink->linkfunction == NULL)
	{
		// Could not get a link function. Fail!
		//if (outlink->linkfunction != NULL && outlink->linkdata != NULL)		free(outlink->linkdata);
		//if (inlink->linkfunction != NULL && inlink->linkdata != NULL)		free(inlink->linkdata);
		free(outlink);
		free(inlink);

		exception_set(err, EINVAL, "Could not find a pair of link functions for link %c -> %c", outsig, insig);
		return false;
	}

	// Now install the link
	list_add(&outlinks->outputs, &outlink->link_list);
	list_add(&inlinks->inputs, &inlink->link_list);

	// TODO - find a better way of sorting, these functions sort the entire list tuple!
	link_sort(outlinks);
	link_sort(inlinks);

	return backing;
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

			link_handle(link->linkfunction, link->linkdata, link->backing, port->backing);
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

			link_handle(link->linkfunction, link->linkdata, port->backing, link->backing);
		}
	}
	mutex_unlock(&io_lock);
}

void link_sort(linklist_t * links)
{
	int link_compare(list_t * a, list_t * b)
	{
		const model_linksymbol_t * la = link_symbol(list_entry(a, link_t, link_list));
		const model_linksymbol_t * lb = link_symbol(list_entry(b, link_t, link_list));

		const char * la_name = NULL, * lb_name = NULL;
		bool la_hasindex = false, lb_hasindex = false;
		model_getlinksymbol(la, NULL, &la_name, &la_hasindex, NULL);
		model_getlinksymbol(lb, NULL, &lb_name, &lb_hasindex, NULL);

		int c1 = strcmp(la_name, lb_name);
		if (c1 != 0)
		{
			return c1;
		}

		// TODO - improve logic
		// Logic sucks here, returns int where signs ensure -no_index'd < +has_index
		return (la_hasindex)? ((lb_hasindex)? 0 : 1) : ((lb_hasindex)? 0 : -1);
	}

	list_sort(&links->inputs, link_compare);
	list_sort(&links->outputs, link_compare);
}



static void copy_b2b(const void * linkdata, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	*(bool *)to = *(const bool *)from;
}

static void copy_i2i(const void * linkdata, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	*(int *)to = *(const int *)from;
}

static void copy_d2d(const void * linkdata, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	*(double *)to = *(const double *)from;
}

static void copy_c2c(const void * linkdata, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	*(char *)to = *(const char *)from;
}

static void copy_s2s(const void * linkdata, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	strncpy(*(char **)to, *(const char **)from, AUL_STRING_MAXLEN - 1);
}

static void copy_x2x(const void * linkdata, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	if (!to_isnull)		buffer_free(*(buffer_t *)to);
	if (!from_isnull)	*(buffer_t *)to = buffer_dup(*(const buffer_t *)from);
}


link_f link_getfunction(const model_linksymbol_t * model_link, char from_sig, char to_sig, void ** linkdata)
{
	// Sanity check
	{
		if (model_link == NULL || linkdata == NULL)
		{
			return NULL;
		}
	}


	void makedata_null()
	{
		*linkdata = NULL;
	}

	void makedata_int(int v)
	{
		*linkdata = malloc(sizeof(int));
		**(int **)linkdata = v;
	}


	switch (from_sig)
	{
		case T_BOOLEAN:
		{
			switch (to_sig)
			{
				case T_BOOLEAN:		makedata_null();		return copy_b2b;
				default:			return NULL;
			}
		}

		case T_INTEGER:
		{
			switch (to_sig)
			{
				case T_INTEGER:		makedata_null();		return copy_i2i;
				default:									return NULL;
			}
		}

		case T_DOUBLE:
		{
			switch (to_sig)
			{
				case T_DOUBLE:		makedata_null();		return copy_d2d;
				default:									return NULL;
			}
		}

		case T_CHAR:
		{
			switch (to_sig)
			{
				case T_CHAR:		makedata_null();		return copy_c2c;
				default:									return NULL;
			}
		}

		case T_STRING:
		{
			switch (to_sig)
			{
				case T_STRING:		makedata_null();		return copy_s2s;
				default:									return NULL;
			}
		}

		case T_ARRAY_BOOLEAN:
		{
			switch (to_sig)
			{
				default:									return NULL;
			}
		}

		case T_ARRAY_INTEGER:
		{
			switch (to_sig)
			{
				default:									return NULL;
			}
		}

		case T_ARRAY_DOUBLE:
		{
			switch (to_sig)
			{
				default:									return NULL;
			}
		}

		case T_BUFFER:
		{
			switch (to_sig)
			{
				case T_BUFFER:		makedata_null();		return copy_x2x;
				default:									return NULL;
			}
		}

		default:
		{
			return NULL;
		}
	}
}
