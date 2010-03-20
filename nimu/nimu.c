#include <string.h>

#include <kernel.h>
#include <serial.h>

MOD_VERSION("0.9");
MOD_AUTHOR("Andrew Klofas <andrew.klofas@senseta.com> and Jacques Dolan <jacques.dolan@gmail.com>");
MOD_DESCRIPTION("Module connects to a USB Memsense nIMU");
MOD_DEPENDENCY( "serial" );
MOD_INIT(module_init);

static int fd = 0;

const char * serial_port = "/dev/ttyUSB0";

CFG_PARAM(serial_port, "s", "Serial port of the Memsense nIMU");


static char buffer[250];
static size_t bufi = 0;
static boolean nimu_newdata(GIOChannel * gio, GIOCondition condition, void * empty)
{
	size_t read = 0;

	GIOStatus status = g_io_channel_read_chars(gio, buffer + bufi, sizeof(buffer) - bufi, &read, NULL);
	if (status != G_IO_STATUS_NORMAL)
	{
		LOG(LOG_INFO, "NIMU DISCONNECT");
		return FALSE;
	}

	/*
	LOG(LOG_INFO, "READ %d, ON %d, PACKET %d", read, bufi, (size_t) buffer[4] & 0xFF);

	bufi += read;

	while (bufi > 5 && bufi >= (size_t) buffer[4])
	{
		size_t len = (size_t) buffer[4];

		if (len == 0)
		{
			LOG(LOG_INFO, "BREAK");
			bufi = 0;
			break;
		}

		LOG(LOG_INFO, "LEN %d BUFI %d", len, bufi);

		size_t i=0;
		for (; i<len; i++)
		{
			LOG(LOG_INFO, "Data[%d]: (0x%x) (%d) %c", i, (int)(buffer[i] & 0xFF), buffer[i], buffer[i]);
		}

		memmove(buffer, buffer + len, bufi - len);
		bufi -= len;
	}
	*/

	LOG(LOG_INFO, "Read %d", read);


	return TRUE;
}

void module_init() {
	fd = serial_open_bin(serial_port, B115200, nimu_newdata);
}
