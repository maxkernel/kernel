

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
	service_h handle = userdata;

	const buffer_t * buffer = INPUT(buffer);
	if (buffer == NULL)
	{
		// No data to process
		return;
	}

	char data[buffer_size(*buffer)];
	size_t read = buffer_read(*buffer, data, 0, sizeof(data));		// TODO - Don't do this!! The buffer might be *huge* and blow the stack!

	if (read != sizeof(data))
	{
		LOG(LOG_WARN, "Could not read all buffer data");
		return;
	}

	service_writedata(handle, kernel_timestamp(), data, sizeof(data));
}
