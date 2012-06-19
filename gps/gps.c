#include <unistd.h>
#include <time.h>
#include <regex.h>

#include <kernel.h>
#include <buffer.h>
#include <aul/parse.h>
#include <aul/serial.h>
#include <aul/mainloop.h>


#define GPS_RETRY_TIMEOUT		(5 * NANOS_PER_SECOND)		// Retry opening port if haven't received GPS data after this long
#define GPS_BUFFER_SIZE			200

typedef struct
{
	char * serial_port;
	speed_t serial_speed;

	int fd;
	bool readsuccess;

	char buffer[GPS_BUFFER_SIZE];
	size_t size;

	mutex_t update_mutex;
	int lastupdate;			// milliseconds when struct was last updated
	bool lock;
	string_t date;
	string_t time;
	double lat;
	double lon;
	double heading;
	double speed;
	double elevation;
	int satellites;
	double hdop;
} gps_t;

// static local vars
static regex_t gpgga_match;
static regex_t gprmc_match;
static regex_t latlong_match;


// TODO - add security around these pbuf[25] vars!
static double mtod(char * start, regmatch_t * m)
{
	char pbuf[25] = {0};
	memcpy(pbuf, start + m->rm_so, m->rm_eo - m->rm_so);
	return parse_double(pbuf, NULL);
}

static int mtoi(char * start, regmatch_t * m)
{
	char pbuf[25] = {0};
	memcpy(pbuf, start + m->rm_so, m->rm_eo - m->rm_so);
	return parse_int(pbuf, NULL);
}

static double mtoll(char * start, regmatch_t * m)
{
	char pbuf[25] = {0};
	memcpy(pbuf, start + m->rm_so, m->rm_eo - m->rm_so);
	
	regmatch_t match[3];
	memset(match, 0, sizeof(regmatch_t) * 3);

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

static bool gps_newdata(mainloop_t * loop, int fd, fdcond_t condition, void * userdata)
{
	gps_t * gps = userdata;
	if (gps == NULL)
	{
		return false;
	}
	
	ssize_t bytesread = read(fd, &gps->buffer[gps->size], sizeof(gps->buffer) - gps->size - 1);
	if (bytesread <= 0)
	{
		//end stream!
		LOG(LOG_WARN, "End of stream reached in GPS serial port %s", gps->serial_port);
		return false;
	}
	
	gps->size += bytesread;
	gps->readsuccess = true;

	char * eol = NULL;
	if ((eol = strchr(gps->buffer, '\n')) != NULL)
	{
		*eol = '\0';	// We now have a complete null-terminated line in our buffer

		regmatch_t match[11];
		memset(match, 0, sizeof(regmatch_t) * 11);
		
		if (regexec(&gpgga_match, gps->buffer, 11, match, 0) == 0)
		{
			mutex_lock(&gps->update_mutex);
			{
				int hh = mtoi(gps->buffer, &match[1]);
				int mm = mtoi(gps->buffer, &match[2]);
				int ss = mtoi(gps->buffer, &match[3]);
				string_set(&gps->time, "%02d:%02d:%02dUTC", hh, mm, ss);

				gps->lat = mtoll(gps->buffer, &match[4]) * (mtoc(gps->buffer, &match[5]) == 'S'? -1 : 1);
				gps->lon = mtoll(gps->buffer, &match[6]) * (mtoc(gps->buffer, &match[7]) == 'W'? -1 : 1);

				gps->elevation = mtod(gps->buffer, &match[10]);
				gps->satellites = mtoi(gps->buffer, &match[8]);
				gps->hdop = mtod(gps->buffer, &match[9]);

				gps->lock = true;
				gps->lastupdate = time(NULL);
			}
			mutex_unlock(&gps->update_mutex);
		}
		else if (regexec(&gprmc_match, gps->buffer, 6, match, 0) == 0)
		{
			mutex_lock(&gps->update_mutex);
			{
				gps->speed = mtod(gps->buffer, &match[1]) * 0.514444444;
				gps->heading = mtod(gps->buffer, &match[2]);

				int dd = mtoi(gps->buffer, &match[3]);
				int mm = mtoi(gps->buffer, &match[4]);
				int yy = mtoi(gps->buffer, &match[5]);
				string_set(&gps->date, "20%02d-%02d-%02dT", yy, mm, dd);
			}
			mutex_unlock(&gps->update_mutex);
		}
		
		// shift the rest of the buffer data to beginning of buffer
		size_t len = strlen(gps->buffer) + 1;
		memmove(gps->buffer, &gps->buffer[len], gps->size - len);
		gps->size -= len;
		
		// clear the out-of-bounds data in the buffer
		memset(&gps->buffer[gps->size], 0, sizeof(gps->buffer) - gps->size);
		
	}
	
	return true;
}

static bool gps_readtimeout(mainloop_t * loop, uint64_t nanoseconds, void * userdata)
{
	gps_t * gps = userdata;

	if (!gps->readsuccess)
	{
		// Have not read any data from serial port in 2 seconds, close and reopen
		// Fixes bug where computer can't read serial data from GPS (FIXME)
		LOG(LOG_WARN, "Haven't received any GPS data from serial port %s. Retrying connection", gps->serial_port);
	
		mainloop_removefdwatch(NULL, gps->fd, FD_READ, NULL);
		close(gps->fd);

		gps->fd = serial_open(gps->serial_port, gps->serial_speed);
		mainloop_addfdwatch(NULL, gps->fd, FD_READ, gps_newdata, NULL, NULL);
		
		return true;
	}
	else
	{
		// We have read something, no need for this timer anymore
		return false;
	}
}

static void gps_update(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	gps_t * gps = object;

	mutex_lock(&gps->update_mutex);
	{
		output(lastupdate, &gps->lastupdate);
		output(lock, &gps->lock);
		output(latitude, &gps->lat);
		output(longitude, &gps->lon);
		output(heading, &gps->heading);
		output(speed, &gps->speed);
		output(elevation, &gps->elevation);
		output(satellites, &gps->satellites);
		output(hdop, &gps->hdop);
	}
	mutex_unlock(&gps->update_mutex);
}

void * gps_new(char * serial_port, int baud)
{
	// Check input
	speed_t speed = serial_getspeed(baud);
	if (speed == 0)
	{
		LOG(LOG_WARN, "Invalid speed setting on GPS device %s: %d", serial_port, baud);
		return NULL;
	}

	gps_t * gps = malloc(sizeof(gps_t));
	memset(gps, 0, sizeof(gps_t));

	gps->serial_port = strdup(serial_port);
	gps->serial_speed = serial_getspeed(baud);
	gps->readsuccess = false;
	mutex_init(&gps->update_mutex, M_RECURSIVE);
	gps->lastupdate = 0;
	gps->lock = false;
	string_clear(&gps->date);
	string_clear(&gps->time);
	gps->lat = gps->lon = 0.0;
	gps->heading = gps->speed = 0;
	gps->elevation = 0.0;
	gps->satellites = 0;
	gps->hdop = 0.0;

	// Set up serial port
	gps->fd = serial_open(serial_port, gps->serial_speed);
	if (gps->fd < 0)
	{
		LOG(LOG_WARN, "Could not open GPS serial device %s", serial_port);
		return NULL;
	}

	// Add gps fd to mainloop
	{
		exception_t * e = NULL;
		if (!mainloop_addfdwatch(NULL, gps->fd, FD_READ, gps_newdata, gps, &e))
		{
			LOG(LOG_ERR, "Could not add GPS fd to mainloop: %s", exception_message(e));
			exception_free(e);
			return NULL;
		}
	}

	// Create a timer to keep track of gps
	{
		// TODO IMPORTANT - we should NOT need this! Find out why we do or if it even helps!
		exception_t * e = NULL;
		if (mainloop_addnewfdtimer(NULL, "GPS read timeout", GPS_RETRY_TIMEOUT, gps_readtimeout, gps, &e) < 0)
		{
			LOG(LOG_ERR, "Could not create GPS read monitor: %s", exception_message(e));
			exception_free(e);
		}
	}

	return gps;
}

static void gps_destroy(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	gps_t * gps = object;
	close(gps->fd);
	free(gps->serial_port);
	free(gps);
}

static bool gps_init() {
	// Set up $GPGGA regex (NMEA protocol)
	// TODO - make this extended REGEX
	if (regcomp(&gpgga_match, "^\\$GPGGA,\\([[:digit:]]\\{2\\}\\)\\([[:digit:]]\\{2\\}\\)\\([[:digit:]]\\{2\\}\\)\\.[[:digit:]]\\{3\\},\\([[:digit:].]\\+\\),"
					"\\([NS]\\),\\([[:digit:].]\\+\\),\\([EW]\\),[12],\\([[:digit:]]\\+\\),\\(\[[:digit:].]\\+\\),\\(\[[:digit:].\\\\-]\\+\\),"
					"M,[[:digit:].\\\\-]\\+,M,,.*$", 0) != 0 ||
	    regcomp(&gprmc_match, "^\\$GPRMC,[[:digit:].]\\+,A,[[:digit:].]\\+,[NS],[[:digit:].]\\+,[EW],\\([[:digit:].]\\+\\),\\([[:digit:].]\\+\\),"
	    				"\\([[:digit:]]\\{2\\}\\)\\([[:digit:]]\\{2\\}\\)\\([[:digit:]]\\{2\\}\\),.*$", 0) != 0 ||
	    regcomp(&latlong_match, "^\\([[:digit:]]\\{2,3\\}\\)\\([[:digit:]]\\{2\\}\\.[[:digit:]]\\{4\\}\\)$", 0) != 0)
	{
		LOG(LOG_ERR, "Could not compile regular expression to match GPS stream!!");
		return false;
	}

	return true;
}


module_name("GPS");
module_version(1,0,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Reads from any serial GPS module outputting NMEA format");
module_oninitialize(gps_init);

define_block(	nmea,	"NMEA GPS device", gps_new, "si", "(1) Serial port [eg. /dev/ttyUSB0], (2) Baud rate of the GPS device [eg. 4800]");
block_onupdate(	nmea,	gps_update);
block_ondestroy(nmea,	gps_destroy);
block_output(	nmea,	lastupdate,	'i',	"The millisecond timestamp for the last GPS reading (default is 0)s");
block_output(	nmea,	lock,		'b',	"True if the device has a GPS lock (default is false)");
//block_output(	nmea,	time,		's'); // TODO - add UTC time as string
block_output(	nmea,	latitude,	'd',	"The latitude reading (default is 0)");
block_output(	nmea,	longitude,	'd',	"The longitude reading (default is 0)");
block_output(	nmea,	heading,	'd', 	"The estimated heading angle in degrees true (default is 0)");
block_output(	nmea,	speed,		'd',	"The estimated speed in knots (default is 0)");
block_output(	nmea,	elevation,	'd',	"Altitide in meters above sea level (default is 0)");
block_output(	nmea,	satellites,	'i',	"Number of satellites being tracked (default is 0)");
block_output(	nmea,	hdop,		'd',	"Horizontal dilution of position");
