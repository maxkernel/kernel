#include <errno.h>

#include <maxmodel/model.h>

#include <kernel.h>
#include <kernel-priv.h>

port_t * port_new(meta_iotype_t type, const char * name, iobacking_t * backing, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}

		if (name == NULL || backing == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}

		if (strlen(name) >= MODEL_SIZE_NAME)
		{
			exception_set(err, EINVAL, "Port name '%s' too long. (MODEL_SIZE_NAME = %d)", name, MODEL_SIZE_NAME);
			return NULL;
		}
	}

	port_t * port = malloc(sizeof(port_t));
	memset(port, 0, sizeof(port_t));
	port->type = type;
	strcpy(port->name, name);
	port->backing = backing;

	return port;
}

port_t * port_lookup(portlist_t * ports, meta_iotype_t type, const char * name)
{
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

bool port_makeblockports(const block_t * block, portlist_t * list, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return false;
		}

		if (block == NULL || list == NULL)
		{
			return false;
		}
	}

	iterator_t ioitr = block_ioitr(block);
	{
		const meta_blockio_t * blockio = NULL;
		while (block_ioitrnext(ioitr, &blockio))
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

			port_t * port = port_new(type, name, iobacking, err);
			if (port == NULL || exception_check(err))
			{
				return false;
			}

			list_add(list, &port->port_list);
		}

		port_sort(list);
	}
	iterator_free(ioitr);

	return true;
}

void port_sort(portlist_t * list)
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

	list_sort(list, port_compare);
}
