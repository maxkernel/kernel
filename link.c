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
	function(data, from->data, from->isnull, to->data, to->isnull);
	to->isnull = from->isnull;
}

iobacking_t * link_connect(const model_link_t * link, char outsig, linklist_t * outlinks, char insig, linklist_t * inlinks, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(link == NULL || outlinks == NULL || inlinks == NULL)
		{
			exception_set(err, EINVAL, "Invalid arguments!");
			return NULL;
		}
	}

	// Get the symbol information for both sides of the connection
	const model_linksymbol_t * outsym = NULL, * insym = NULL;
	model_getlink(link, &outsym, &insym);
	if (outsym == NULL || insym == NULL)
	{
		exception_set(err, EINVAL, "Malformed link (null terminal)");
		return NULL;
	}

	// Make sure that no one else has linked to same input
	{
		list_t * pos = NULL;
		list_foreach(pos, &inlinks->inputs)
		{
			link_t * testlink = list_entry(pos, link_t, link_list);
			const model_linksymbol_t * testsym = testlink->symbol;

			if (strcmp(insym->name, testsym->name) == 0)
			{
				// Insym and testsym have same names, make sure that they point to different memory
				if (!insym->attrs.indexed && !testsym->attrs.indexed)
				{
					exception_set(err, EINVAL, "Input %s already linked!", insym->name);
					return NULL;
				}
				else if (insym->attrs.indexed && testsym->attrs.indexed && insym->index == testsym->index)
				{
					exception_set(err, EINVAL, "Input %s[%zu] already linked!", insym->name, insym->index);
					return NULL;
				}
				else if (!insym->attrs.indexed && testsym->attrs.indexed)
				{
					exception_set(err, EINVAL, "Mixing array and array index links not supported. (Although it can be, so be vocal about it if you want it)");
					return NULL;
				}
			}
		}
	}

	// Determine the intermediary data type
	char sig = '\0';

	bool match(char intype, bool inindexed, char outtype, bool outindexed, char sigtype)
	{
		if (sig == '\0' &&
			insig == intype && insym->attrs.indexed == inindexed &&
			outsig == outtype && outsym->attrs.indexed == outindexed
		)
		{
			sig = sigtype;
			return true;
		}

		return false;
	}

	bool matchi(char intype, bool inindexed, char outtype, bool outindexed, char sigtype, const char * info)
	{
		if (match(intype, inindexed, outtype, outindexed, sigtype))
		{
			LOGK(LOG_INFO, "Linking %s(%c) -> %s(%c): %s", outsym->name, outsig, insym->name, insig, info);
			return true;
		}

		return false;
	}

	// Match boolean -> ?
	match(	T_BOOLEAN,			false,		T_BOOLEAN,			false,		T_BOOLEAN);
	match(	T_BOOLEAN,			false,		T_INTEGER,			false,		T_INTEGER);
	matchi(	T_BOOLEAN,			false,		T_DOUBLE,			false,		T_DOUBLE,		"Unusual link type");
	match(	T_BOOLEAN,			false,		T_ARRAY_BOOLEAN,	true,		T_BOOLEAN);
	match(	T_BOOLEAN,			false,		T_ARRAY_INTEGER,	true,		T_INTEGER);
	matchi(	T_BOOLEAN,			false,		T_ARRAY_DOUBLE,		true,		T_DOUBLE,		"Unusual link type");

	// Match integer -> ?
	matchi(	T_INTEGER,			false,		T_BOOLEAN,			false,		T_BOOLEAN,		"Unusual link type");
	match(	T_INTEGER,			false,		T_INTEGER,			false,		T_INTEGER);
	match(	T_INTEGER,			false,		T_DOUBLE,			false,		T_DOUBLE);
	match(	T_INTEGER,			false,		T_ARRAY_INTEGER,	true,		T_INTEGER);
	match(	T_INTEGER,			false,		T_ARRAY_DOUBLE,		true,		T_DOUBLE);
	matchi(	T_INTEGER,			false,		T_ARRAY_BOOLEAN,	true,		T_BOOLEAN,		"Unusual link type");

	// Match double -> ?
	matchi(	T_DOUBLE,			false,		T_BOOLEAN,			false,		T_BOOLEAN,		"Unusual link type");
	match(	T_DOUBLE,			false,		T_DOUBLE,			false,		T_DOUBLE);
	matchi(	T_DOUBLE,			false,		T_INTEGER,			false,		T_INTEGER,		"Possible loss of precision");
	matchi(	T_DOUBLE,			false,		T_ARRAY_BOOLEAN,	true,		T_BOOLEAN,		"Unusual link type");
	matchi(	T_DOUBLE,			false,		T_ARRAY_INTEGER,	true,		T_INTEGER,		"Possible loss of precision");
	match(	T_DOUBLE,			false,		T_ARRAY_DOUBLE,		true,		T_DOUBLE);

	// Match char -> ?
	matchi(	T_CHAR,				false,		T_INTEGER,			false,		T_INTEGER,		"Unusual link type");
	match(	T_CHAR,				false,		T_CHAR,				false,		T_CHAR);
	match(	T_CHAR,				false,		T_STRING,			true,		T_CHAR);

	// Match string -> ?
	matchi(	T_STRING,			true,		T_INTEGER,			false,		T_INTEGER,		"Unusual link type");
	matchi( T_STRING,			true,		T_CHAR,				false,		T_CHAR,			"Unusual link type");
	match(	T_STRING,			false,		T_STRING,			false,		T_STRING);

	// Match boolean array -> ?
	match(	T_ARRAY_BOOLEAN,	true,		T_BOOLEAN,			false,		T_BOOLEAN);
	match(	T_ARRAY_BOOLEAN,	true,		T_INTEGER,			false,		T_BOOLEAN);
	matchi(	T_ARRAY_BOOLEAN,	true,		T_DOUBLE,			false,		T_BOOLEAN,		"Unusual link type");
	match(	T_ARRAY_BOOLEAN,	false,		T_ARRAY_BOOLEAN,	false,		T_ARRAY_BOOLEAN);
	match(	T_ARRAY_BOOLEAN,	true,		T_ARRAY_BOOLEAN,	true,		T_BOOLEAN);
	match(	T_ARRAY_BOOLEAN,	true,		T_ARRAY_INTEGER,	true,		T_BOOLEAN);
	matchi(	T_ARRAY_BOOLEAN,	true,		T_ARRAY_DOUBLE,		true,		T_BOOLEAN,		"Unusual link type");

	// Match integer array -> ?
	matchi(	T_ARRAY_INTEGER,	true,		T_BOOLEAN,			false,		T_INTEGER,		"Unusual link type and possible loss of precision");
	match(	T_ARRAY_INTEGER,	true,		T_INTEGER,			false,		T_INTEGER);
	match(	T_ARRAY_INTEGER,	true,		T_DOUBLE,			false,		T_INTEGER);
	match(	T_ARRAY_INTEGER,	false,		T_ARRAY_INTEGER,	false,		T_ARRAY_INTEGER);
	match(	T_ARRAY_INTEGER,	true,		T_ARRAY_INTEGER,	true,		T_INTEGER);

	// Match double array -> ?
	matchi(	T_ARRAY_DOUBLE,		true,		T_BOOLEAN,			false,		T_DOUBLE,		"Unusual link type and possible loss of precision");
	matchi(	T_ARRAY_DOUBLE,		true,		T_INTEGER,			false,		T_DOUBLE,		"Possible loss of precision");
	match(	T_ARRAY_DOUBLE,		true,		T_DOUBLE,			false,		T_DOUBLE);
	match(	T_ARRAY_DOUBLE,		false,		T_ARRAY_DOUBLE,		false,		T_ARRAY_DOUBLE);
	match(	T_ARRAY_DOUBLE,		true,		T_ARRAY_DOUBLE,		true,		T_DOUBLE);

	// Match buffer -> ?
	match(	T_BUFFER,			false,		T_BUFFER,			false,		T_BUFFER);


	if (sig == '\0')
	{
		exception_set(err, EINVAL, "Could not find a proper link solution for %c -> %c", outsig, insig);
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

void link_destroy(linklist_t * links)
{
	// Sanity check
	{
		if unlikely(links == NULL)
		{
			return;
		}
	}

	// Free input links
	{
		list_t * pos = NULL, * n = NULL;
		list_foreach_safe(pos, n, &links->inputs)
		{
			link_t * link = list_entry(pos, link_t, link_list);
			list_remove(pos);

			// Don't free link->backing. It is a shared backing between output and input.

			if (link->linkdata != NULL)
			{
				free(link->linkdata);
			}
			free(link);
		}
	}

	// Free output links
	{
		list_t * pos = NULL, * n = NULL;
		list_foreach_safe(pos, n, &links->outputs)
		{
			link_t * link = list_entry(pos, link_t, link_list);
			list_remove(pos);

			// Don't free link->backing. It is a shared backing between output and input.

			if (link->linkdata != NULL)
			{
				free(link->linkdata);
			}
			free(link);
		}
	}
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
	unused(linkdata); unused(from_isnull); unused(to_isnull);

	*(bool *)to = *(const bool *)from;
}

static void copy_i2i(const void * linkdata, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	unused(linkdata); unused(from_isnull); unused(to_isnull);

	*(int *)to = *(const int *)from;
}

static void copy_i2d(const void * linkdata, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	unused(linkdata); unused(from_isnull); unused(to_isnull);

	*(double *)to = (double)*(const int *)from;
}

static void copy_d2d(const void * linkdata, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	unused(linkdata); unused(from_isnull); unused(to_isnull);

	*(double *)to = *(const double *)from;
}

static void copy_d2i(const void * linkdata, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	unused(linkdata); unused(from_isnull); unused(to_isnull);

	*(int *)to = (int)*(const double *)from;
}

static void copy_c2c(const void * linkdata, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	unused(linkdata); unused(from_isnull); unused(to_isnull);

	*(char *)to = *(const char *)from;
}

static void copy_s2s(const void * linkdata, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	unused(linkdata); unused(from_isnull); unused(to_isnull);

	strncpy(*(char **)to, *(const char **)from, AUL_STRING_MAXLEN - 1);
}

static void copy_x2x(const void * linkdata, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	unused(linkdata);

	if (!to_isnull)		buffer_free(*(buffer_t **)to);
	if (!from_isnull)	*(buffer_t **)to = buffer_dup(*(const buffer_t **)from);
}

static void copy_d2D(const void * linkdata, const void * from, bool from_isnull, void * to, bool to_isnull)
{
	if (from_isnull)	return;
	if (to_isnull)		*(array_t **)to = array_new();
	array_writeindex(*(array_t **)to, T_ARRAY_DOUBLE, *(int *)linkdata, from);
}


link_f link_getfunction(const model_linksymbol_t * linksym, char fromsig, char tosig, void ** linkdata)
{
	// Sanity check
	{
		if (linksym == NULL || linkdata == NULL)
		{
			return NULL;
		}
	}

	#define func_match(fromtype, totype, data, func) \
		({ if (fromsig == fromtype && tosig == totype) { data; return func; } })

	#define data_null()		({ *linkdata = NULL; })
	#define data_index()	({ if (!linksym->attrs.indexed) { return NULL; } *linkdata = malloc(sizeof(size_t)); **(size_t **)linkdata = linksym->index; })


	// Match boolean -> ?
	func_match(	T_BOOLEAN,			T_BOOLEAN,			data_null(),	copy_b2b	);

	// Match integer -> ?
	func_match(	T_INTEGER,			T_INTEGER,			data_null(),	copy_i2i	);
	func_match(	T_INTEGER,			T_DOUBLE,			data_null(),	copy_i2d	);

	// Match double -> ?
	func_match(	T_DOUBLE,			T_DOUBLE,			data_null(),	copy_d2d	);
	func_match(	T_DOUBLE,			T_INTEGER,			data_null(),	copy_d2i	);
	func_match(	T_DOUBLE,			T_ARRAY_DOUBLE,		data_index(),	copy_d2D	);

	// Match char -> ?
	func_match(	T_CHAR,				T_CHAR,				data_null(),	copy_c2c	);

	// Match string -> ?
	func_match(	T_STRING,			T_STRING,			data_null(),	copy_s2s	);

	// Match boolean array -> ?
	func_match(	T_ARRAY_BOOLEAN,	T_ARRAY_BOOLEAN,	data_null(),	copy_x2x	);

	// Match integer array -> ?
	func_match(	T_ARRAY_INTEGER,	T_ARRAY_INTEGER,	data_null(),	copy_x2x	);

	// Match double array -> ?
	func_match(	T_ARRAY_DOUBLE,		T_ARRAY_DOUBLE,		data_null(),	copy_x2x	);

	// Match buffer -> ?
	func_match(	T_BUFFER,			T_BUFFER,			data_null(),	copy_x2x	);


	// No matches? Return NULL
	return NULL;
}
