#include <aul/exception.h>

#include <service.h>
#include <array.h>
#include <kernel.h>


static void * doublearray_new(const char * name, const char * format, const char * desc)
{
	exception_t * e = NULL;
	service_t * service = service_new(name, format, desc, &e);
	if (service == NULL || exception_check(&e))
	{
		LOG(LOG_ERR, "Could not create new double array service: %s", exception_message(e));
		exception_free(e);
		return NULL;
	}

	return service;
}

static void doublearray_update(void * object)
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

define_block(double, "Make a double array available as a service", doublearray_new, "sss", "(1) Name of the service. (2) Format [eg. TXT or JPEG]. (3) The description");
block_onupdate(double, doublearray_update);
block_input(double, array, T_ARRAY_DOUBLE, "Input data double array");
block_output(double, clients, T_INTEGER, "Number of connected clients");
