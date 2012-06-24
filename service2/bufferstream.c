#include <aul/exception.h>

#include <service.h>
#include <buffer.h>
#include <kernel.h>


static void * bufferservice_new(const char * name, const char * format, const char * desc)
{
	exception_t * e = NULL;
	service_t * service = service_new(name, format, desc, &e);
	if (service == NULL || exception_check(&e))
	{
		LOG(LOG_ERR, "Could not create new buffer service: %s", exception_message(e));
		exception_free(e);
		return NULL;
	}

	return service;
}

static void bufferservice_update(void * object)
{
	service_t * service = object;
	if (service == NULL)
	{
		return;
	}

	// Grab input buffer and dispatch it to service subsystem
	buffer_t * const * buffer = input(buffer);
	if (buffer != NULL)
	{
		service_send(service, kernel_timestamp(), *buffer);
	}

	// Output the number of connected clients
	int numclients = service_numclients(service);
	output(clients, &numclients);
}

define_block(buffer, "Make the input buffer available as a service", bufferservice_new, "sss", "(1) Name of the service. (2) Format [eg. TXT or JPEG]. (3) The description");
block_onupdate(buffer, bufferservice_update);
block_input(buffer, buffer, T_BUFFER, "Input data buffer");
block_output(buffer, clients, T_INTEGER, "Number of connected clients");
