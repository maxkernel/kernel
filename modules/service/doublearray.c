#include <aul/exception.h>

#include <service.h>
#include <array.h>
#include <kernel.h>


static void * doubles_new(const char * name, const char * desc)
{
	exception_t * e = NULL;
	service_t * service = service_new(name, "doubles", desc, &e);
	if (service == NULL || exception_check(&e))
	{
		LOG(LOG_ERR, "Could not create new double array service: %s", exception_message(e));
		exception_free(e);
		return NULL;
	}

	return service;
}

static void doubles_update(void * object)
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

define_block(doubles, "Make a double array available as a service", doubles_new, "ss", "(1) Name of the service. (2) The description of the service");
block_onupdate(doubles, doubles_update);
block_input(doubles, array, T_ARRAY_DOUBLE, "Input data double array");
block_output(doubles, clients, T_INTEGER, "Number of connected clients");
