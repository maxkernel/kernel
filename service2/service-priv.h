
#include <aul/exception.h>
#include <aul/mainloop.h>

#include <kernel.h>

#include <service.h>


#define SERVICE_CLIENTS_PER_STREAM		25
#define SERVICE_NUM_PACKETS				50

typedef struct
{
	list_t packet_list;

	service_t * service;
	int64_t timestamp;
	buffer_t buffer;
} packet_t;


bool client_check(client_t * client);

// Private subsystems
bool tcp_init(exception_t ** err);
bool udp_init(exception_t ** err);

