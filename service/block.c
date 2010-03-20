

#include <kernel.h>
#include <buffer.h>
#include "service.h"


DEF_BLOCK(bufferstream, s_bufstream_new, "ssss", "Create a new buffer stream service");
BLK_INPUT(bufferstream, buffer, S_BUFFER, "Input buffer to stream to service clients");
BLK_ONUPDATE(bufferstream, s_bufstream_update);

void * s_bufstream_new(const char * id, const char * name, const char * format, const char * desc)
{
	service_h handle = service_register(id, name, format, NULL, desc, NULL, NULL, NULL);
	return (void *)handle;
}

void s_bufstream_update(void * userdata)
{
	if (ISNULL(buffer))
	{
		return;
	}

	service_h handle = userdata;
	buffer_t buffer = INPUTT(buffer_t, buffer);
	service_writedata(handle, kernel_timestamp(), buffer_data(buffer), buffer_datasize(buffer));
}
