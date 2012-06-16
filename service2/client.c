#include <errno.h>

#include <kernel.h>

#include <service.h>
#include "service-priv.h"

ssize_t client_control(client_t * client, void * buffer, size_t length)
{
	// Sanity check
	{
		if unlikely(client == NULL)
		{
			return -1;
		}
	}

	if (length < sizeof(uint8_t))
	{
		// Nothing to read
		return 0;
	}

	uint8_t code = *(uint8_t *)buffer;
	switch (code)
	{
		case SC_GOODBYE:
		{
			return -1;
		}

		case SC_AUTH:
		{
			LOG(LOG_INFO, "Service client AUTH not supported yet.");
			return -1;
		}

		case SC_HEARTBEAT:
		{
			client_lastheartbeat(client) = kernel_elapsed();
			return sizeof(uint8_t);
		}

		case SC_DATA:
		{
			LOG(LOG_WARN, "Invalid control code received from stream client (SC_DATA)");
			return -1;
		}

		case SC_SUBSCRIBE:
		{
			// TODO - finish me
			return 0;
		}

		case SC_UNSUBSCRIBE:
		{
			// TODO - finish me
			return 0;
		}

		case SC_LISTXML:
		{

		}

		default:
		{
			LOG(LOG_WARN, "Unknown control code received from stream client: %#x", code);
			return -1;
		}
	}
}
