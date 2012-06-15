
#include <aul/exception.h>
#include <aul/mainloop.h>

#include <kernel.h>

#include <service.h>


typedef struct
{
	list_t packet_list;

	service_t * service;
	uint64_t timestamp;
	buffer_t buffer;
} packet_t;



// Private subsystems
bool tcp_init(exception_t ** err);
bool udp_init(exception_t ** err);

