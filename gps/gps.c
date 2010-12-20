#include <unistd.h>
#include <regex.h>

#include <kernel.h>
#include <buffer.h>
#include <aul/serial.h>
#include <aul/mainloop.h>


MOD_VERSION("0.1");
MOD_AUTHOR("Andrew Klofas <aklofas@gmail.com>");
MOD_DESCRIPTION("Reads from a serial GPS module outputting NMEA format");
MOD_INIT(module_init);


char * serial_port				= "/dev/ttyUSB0";


//config values
CFG_PARAM(serial_port, "s");


BLK_OUTPUT(STATIC, stream, "x");
BLK_ONUPDATE(STATIC, gps_update);


// static local vars
static bool readsuccess = false;
static int gpsfd = 0;

static regex_t gpgga_match;
static regex_t gprms_match;
static regex_t latlong_match;

static mutex_t stream_mutex;
static String stream = STRING_INIT;

static double mtod(char * start, regmatch_t * m)
{
	char pbuf[25] = {0};
	memcpy(pbuf, start + m->rm_so, m->rm_eo - m->rm_so);
	return strtod(pbuf, NULL);
}

static int mtoi(char * start, regmatch_t * m)
{
	char pbuf[25] = {0};
	memcpy(pbuf, start + m->rm_so, m->rm_eo - m->rm_so);
	return atoi(pbuf);
}

static double mtoll(char * start, regmatch_t * m)
{
	char pbuf[25] = {0};
	memcpy(pbuf, start + m->rm_so, m->rm_eo - m->rm_so);
	
	regmatch_t match[3];
	ZERO(match);
	if (regexec(&latlong_match, pbuf, 3, match, 0) == 0)
	{
		return (double)mtoi(pbuf, &match[1]) + mtod(pbuf, &match[2])/60.0;
	}
	else
	{
		LOG(LOG_WARN, "GPS failed to parse lat/long '%s'", pbuf);
		return 0.0;
	}
}

static inline char mtoc(char * start, regmatch_t * match)
{
	return start[match->rm_so];
}

static bool gps_newdata(int fd, fdcond_t condition, void * userdata)
{
	static char buffer[200] = {0};
	static char line[200] = {0};
	static size_t buffer_size = 0;
	
	ssize_t numread;
	numread = read(fd, buffer+buffer_size, sizeof(buffer)-buffer_size-1);
	if (numread <= 0)
	{
		//end stream!
		LOG(LOG_WARN, "End of stream reached in GPS serial port %s", serial_port);
		return false;
	}
	buffer_size += numread;
	readsuccess = true;
	
	char * eol = NULL;
	if ((eol = strchr(buffer, '\n')) != NULL)
	{
		size_t len = eol - buffer + 1;
		
		// copy the line into the line buffer
		memcpy(line, buffer, len);
		
		{
			static bool rmsline = false;
			static String date = STRING_INIT;
			static double speed = 0.0;
			static double heading = 0.0;
			
			regmatch_t match[11];
			ZERO(match);
			
			if (rmsline && regexec(&gpgga_match, line, 11, match, 0) == 0)
			{
				int hh = mtoi(line, &match[1]);
				int mm = mtoi(line, &match[2]);
				int ss = mtoi(line, &match[3]);
				
				double lat = mtoll(line, &match[4]) * (mtoc(line, &match[5]) == 'S'? -1 : 1);
				double lon = mtoll(line, &match[6]) * (mtoc(line, &match[7]) == 'W'? -1 : 1);
				
				double elev = mtod(line, &match[10]);
				int sat = mtoi(line, &match[8]);
				double hdop = mtod(line, &match[9]);
				
				mutex_lock(&stream_mutex);
				{
					string_clear(&stream);
					string_append(&stream, "time=%s%02d:%02d:%02dUTC,lat=%f,lon=%f,heading=%f,speed=%f,elevation=%f,satellites=%d,hdop=%f",
								date.string, hh, mm, ss, lat, lon, heading, speed, elev, sat, hdop);
				}
				mutex_unlock(&stream_mutex);
			}
			else if (regexec(&gprms_match, line, 6, match, 0) == 0)
			{
				speed = mtod(line, &match[1]) * 0.514444444;
				heading = mtod(line, &match[2]);
				
				int dd = mtoi(line, &match[3]);
				int mm = mtoi(line, &match[4]);
				int yy = mtoi(line, &match[5]);
				string_clear(&date);
				string_append(&date, "20%02d-%02d-%02dT", yy, mm, dd);
				
				rmsline = true;
			}
		}
		
		// clear the line buffer
		memset(line, 0, sizeof(line));
		
		// shift the rest of the buffer data to beginning of buffer
		memmove(buffer, buffer+len, buffer_size-len);
		buffer_size -= len;
		
		// clear the out-of-bounds data in the buffer
		memset(buffer+buffer_size, 0, sizeof(buffer)-buffer_size);
		
	}
	
	return true;
}

static bool gps_readtimeout()
{
	if (!readsuccess)
	{
		// Have not read any data from serial port in 2 seconds, close and reopen
		// fixes bug where computer can't read serial data from GPS
		LOG(LOG_WARN, "Haven't received any GPS data from serial port '%s'. Retrying connection", serial_port);
	
		mainloop_removewatch(NULL, gpsfd, FD_READ);
		close(gpsfd);
		gpsfd = serial_open(serial_port, B4800);
		mainloop_addwatch(NULL, gpsfd, FD_READ, gps_newdata, NULL);
		
		return true;
	}
	else
	{
		// We have read something, no need for this timer anymore
		return false;
	}
}

void gps_update(void * object)
{
	mutex_lock(&stream_mutex);
	{
		if (stream.length > 0)
		{
			buffer_t buf = buffer_new(stream.length);
			memcpy(buffer_data(buf), stream.string, stream.length);
			OUTPUT_NOCOPY(stream, &buf);
		}
	}
	mutex_unlock(&stream_mutex);
}

void module_init() {
	// Init the mutex and string stream
	mutex_init(&stream_mutex, PTHREAD_MUTEX_NORMAL);
	string_clear(&stream);
	
	// Set up $GPGGA regex (NMEA protocol)
	if (regcomp(&gpgga_match, "^\\$GPGGA,\\([[:digit:]]\\{2\\}\\)\\([[:digit:]]\\{2\\}\\)\\([[:digit:]]\\{2\\}\\)\\.[[:digit:]]\\{3\\},\\([[:digit:].]\\+\\),"
					"\\([NS]\\),\\([[:digit:].]\\+\\),\\([EW]\\),[12],\\([[:digit:]]\\+\\),\\(\[[:digit:].]\\+\\),\\(\[[:digit:].\\\\-]\\+\\),"
					"M,[[:digit:].\\\\-]\\+,M,,.*$", 0) != 0 ||
	    regcomp(&gprms_match, "^\\$GPRMC,[[:digit:].]\\+,A,[[:digit:].]\\+,[NS],[[:digit:].]\\+,[EW],\\([[:digit:].]\\+\\),\\([[:digit:].]\\+\\),"
	    				"\\([[:digit:]]\\{2\\}\\)\\([[:digit:]]\\{2\\}\\)\\([[:digit:]]\\{2\\}\\),.*$", 0) != 0 ||
	    regcomp(&latlong_match, "^\\([[:digit:]]\\{2,3\\}\\)\\([[:digit:]]\\{2\\}\\.[[:digit:]]\\{4\\}\\)$", 0) != 0)
	{
		LOG(LOG_FATAL, "Could not compile regular expression to match GPS stream!!");
	}
	
	// Set up serial port
	gpsfd = serial_open(serial_port, B4800);	
	if (gpsfd != -1)
	{
		mainloop_addwatch(NULL, gpsfd, FD_READ, gps_newdata, NULL);
		mainloop_addtimer(NULL, "GPS read timeout", NANOS_PER_SECOND * 2, gps_readtimeout, NULL);
	}
	else
	{
		LOG(LOG_WARN, "Could not open GPS serial device '%s'", serial_port);
	}
}
