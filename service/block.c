#include <kernel.h>
#include <buffer.h>
#include "service.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


void * s_bufstream_new(const char * id, const char * name, const char * format, const char * desc)
{
	service_h handle = service_register(id, name, format, NULL, desc, NULL, NULL, NULL);
	return (void *)handle;
}

void s_bufstream_update(void * userdata)
{
	service_h handle = userdata;

	const buffer_t * buffer = input(buffer);
	if (buffer == NULL)
	{
		// No data to process
		return;
	}

	service_writedata(handle, kernel_timestamp(), *buffer);
}


define_block(bufferstream, "Create a new buffer stream service", s_bufstream_new, "ssss", "(1) ID (2) Name (3) Format (4) Description");
block_input(bufferstream, buffer, 'x', "Input buffer to stream to service clients");
block_onupdate(bufferstream, s_bufstream_update);
