#include <aul/exception.h>

#include <service.h>
#include <array.h>
#include <kernel.h>


static void * bools_new(const char * name, const char * desc)
{
	exception_t * e = NULL;
	service_t * service = service_new(name, "bools", desc, &e);
	if (service == NULL || exception_check(&e))
	{
		LOG(LOG_ERR, "Could not create new boolean array service: %s", exception_message(e));
		exception_free(e);
		return NULL;
	}

	return service;
}

static void bools_update(void * object)
{
	service_t * service = object;
	if (service == NULL)
	{
		return;
	}

	// Grab input buffer and dispatch it to service subsystem
	array_t * const * array = input(array);
	if (array != NULL)
	{
		service_send(service, kernel_timestamp(), *array);
	}

	// Output the number of connected clients
	int numclients = service_numclients(service);
	output(clients, &numclients);
}

define_block(bools, "Make a boolean array available as a service", bools_new, "ss", "(1) Name of the service. (2) The description of the service");
block_onupdate(bools, bools_update);
block_input(bools, array, T_ARRAY_BOOLEAN, "Input data boolean array");
block_output(bools, clients, T_INTEGER, "Number of connected clients");
