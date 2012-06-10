#include <errno.h>

#include <maxmodel/model.h>

#include <kernel.h>
#include <kernel-priv.h>


static void port_sort(portlist_t * ports)
{
	int port_compare(list_t * a, list_t * b)
	{
		port_t * pa = list_entry(a, port_t, port_list);
		port_t * pb = list_entry(b, port_t, port_list);

		int c1 = strcmp(pa->name, pb->name);
		if (c1 != 0)
		{
			return c1;
		}

		// TODO - improve logic
		// Logic sucks here, returns int where signs ensure -input < +output
		return (pa->type == meta_input)? ((pb->type == meta_input)? 0 : -1) : ((pb->type == meta_output)? 0 : 1);
	}

	list_sort(ports, port_compare);
}

bool port_add(portlist_t * ports, meta_iotype_t type, const char * name, iobacking_t * backing, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(ports == NULL || name == NULL || backing == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return false;
		}

		if unlikely(strlen(name) >= MODEL_SIZE_NAME)
		{
			exception_set(err, EINVAL, "Port name '%s' too long. (MODEL_SIZE_NAME = %d)", name, MODEL_SIZE_NAME);
			return false;
		}
	}

	port_t * port = malloc(sizeof(port_t));
	memset(port, 0, sizeof(port_t));
	port->type = type;
	strcpy(port->name, name);
	port->backing = backing;

	list_add(ports, &port->port_list);
	port_sort(ports);
	return true;
}

void port_destroy(portlist_t * ports)
{
	// Sanity check
	{
		if unlikely(ports == NULL)
		{
			return;
		}
	}

	list_t * pos = NULL, * n = NULL;
	list_foreach_safe(pos, n, ports)
	{
		port_t * port = list_entry(pos, port_t, port_list);
		list_remove(pos);

		iobacking_destroy(port->backing);
		free(port);
	}
}

port_t * port_lookup(portlist_t * ports, meta_iotype_t type, const char * name)
{
	// Sanity check
	{
		if unlikely(ports == NULL || name == NULL)
		{
			return NULL;
		}
	}

	list_t * pos = NULL;
	list_foreach(pos, ports)
	{
		port_t * port = list_entry(pos, port_t, port_list);
		if (port->type == type && strcmp(port->name, name) == 0)
		{
			return port;
		}
	}

	return NULL;
}

bool port_makeblockports(const block_t * block, portlist_t * ports, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return false;
		}

		if unlikely(block == NULL || ports == NULL)
		{
			return false;
		}
	}

	iterator_t ioitr = block_ioitr(block);
	{
		const meta_blockio_t * blockio = NULL;
		while (block_ionext(ioitr, &blockio))
		{
			const char * name = NULL;
			meta_iotype_t type = meta_unknownio;
			char sig = '\0';
			meta_getblockio(blockio, NULL, &name, &type, &sig, NULL);

			iobacking_t * iobacking = iobacking_new(sig, err);
			if (iobacking == NULL || exception_check(err))
			{
				return false;
			}

			bool success = port_add(ports, type, name, iobacking, err);
			if (!success || exception_check(err))
			{
				return false;
			}
		}
	}
	iterator_free(ioitr);

	return true;
}
