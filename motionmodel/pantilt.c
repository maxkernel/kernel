#include <math.h>

#include <aul/common.h>

#include <map.h>
#include <kernel.h>


typedef struct
{
	struct
	{
		int pan_min, pan_forward, pan_backward, pan_max;
		int tilt_min, tilt_center, tilt_max;
	} cal;

	struct
	{
		int pan, tilt;
	} pwm;

	struct
	{
		map_t * pan, * tilt;
	} map;
} pantilt_t;

static void setmaps(pantilt_t * pt)
{
	{ // Pan map
		if (pt->map.pan != NULL) map_destroy(pt->map.pan);
		double pan_from[] = {-M_PI, 0.0, M_PI/2.0, M_PI/2.0+0.0001, M_PI};
		double pan_to[] = {pt->cal.pan_backward, pt->cal.pan_forward, pt->cal.pan_max, pt->cal.pan_min, pt->cal.pan_backward};
		pt->map.pan = map_newfromarray(MAP_LINEAR, pan_from, pan_to, 5);
	}

	{ // Tilt map
		if (pt->map.tilt != NULL) map_destroy(pt->map.tilt);
		double tilt_from[] = {-M_PI/4.0, 0.0, M_PI/2.0};
		double tilt_to[] = {pt->cal.tilt_min, pt->cal.tilt_center, pt->cal.tilt_max};
		pt->map.tilt = map_newfromarray(MAP_LINEAR, tilt_from, tilt_to, 3);
	}
}

static void calpreview(void * object, const char * domain, const char * name, const char sig, void * backing, char * hint, size_t hint_length)
{
	pantilt_t * pt = object;

	if		(strprefix(name, "pan"))		pt->pwm.pan		= *(int *)backing;
	else if	(strprefix(name, "tilt"))		pt->pwm.tilt	= *(int *)backing;
}

static void calmodechange(void * object, calmode_t mode, calstatus_t status)
{
	pantilt_t * pt = object;

	if (mode == calmode_runtime)
	{
		// Just completed calibration, reset the maps
		setmaps(pt);
	}
}

static void pantilt_update(void * object)
{
	// Sanity check
	{
		if (object == NULL)
		{
			return;
		}
	}

	pantilt_t * pt = object;

	if (cal_getmode() == calmode_runtime)
	{
		// Not in calibration mode, read inputs and apply maps

		const double * value_pan = input(pan);
		const double * value_tilt = input(tilt);

		if (value_pan != NULL)
		{
			double pan = *value_pan;
			while (pan > M_PI) { pan -= 2.0*M_PI; }
			while (pan < -M_PI) { pan += 2.0*M_PI; }

			pt->pwm.pan = (int)map_tovalue(pt->map.pan, pan);
		}

		if (value_tilt != NULL)
			pt->pwm.tilt = (int)map_tovalue(pt->map.tilt, clamp(*value_tilt, -M_PI/4.0, M_PI/2.0));
	}

	output(pan_pwm, &pt->pwm.pan);
	output(tilt_pwm, &pt->pwm.tilt);
}

static void * pantilt_new()
{
	pantilt_t * pt = malloc(sizeof(pantilt_t));
	memset(pt, 0, sizeof(pantilt_t));

	// Set the default calibration settings
	pt->cal.pan_min		= 1500;		pt->cal.pan_forward		= 1800;		pt->cal.pan_backward	= 1600;		pt->cal.pan_max		= 1900;
	pt->cal.tilt_min	= 1400;		pt->cal.tilt_center		= 1500;		pt->cal.tilt_max		= 1700;

	// Register with the calibration subsystem
	cal_register(	NULL,	"pan_min",		&pt->cal.pan_min,		'i',		"[0:15000] Head Pan Minimum (left -270 deg)",	calpreview,	pt);
	cal_register(	NULL,	"pan_forward",	&pt->cal.pan_forward,	'i',		"[0:15000] Head Pan Center Forward",			calpreview,	pt);
	cal_register(	NULL,	"pan_backward",	&pt->cal.pan_backward,	'i',		"[0:15000] Head Pan Center Backward",			calpreview,	pt);
	cal_register(	NULL,	"pan_max",		&pt->cal.pan_max,		'i',		"[0:15000] Head Pan Maximum (right +90 deg)",	calpreview,	pt);
	cal_register(	NULL,	"tilt_min",		&pt->cal.tilt_min,		'i',		"[0:15000] Head Tilt Minimum (down -45 deg)",	calpreview,	pt);
	cal_register(	NULL,	"tilt_center",	&pt->cal.tilt_center,	'i',		"[0:15000] Head Tilt Center (level forward)",	calpreview,	pt);
	cal_register(	NULL,	"tilt_max",		&pt->cal.tilt_max,		'i',		"[0:15000] Head Tilt Maximum (up +90 deg)",		calpreview,	pt);
	cal_onmodechange(calmodechange,	pt);

	// Set the initial pwm outputs
	pt->pwm.pan = pt->cal.pan_forward;
	pt->pwm.tilt = pt->cal.tilt_center;

	// Initialize the calibration mappings
	setmaps(pt);

	return pt;
}

static void pantilt_destroy(void * object)
{
	// Sanity check
	{
		if (object == NULL)
		{
			return;
		}
	}

	pantilt_t * pt = object;
	map_destroy(pt->map.pan);
	map_destroy(pt->map.tilt);
	free(pt);
}


module_dependency("map");

define_block(	pantilt,	"Pan-tilt turret model. Includes caibration", pantilt_new, "", "No constructor parameters");
block_onupdate(	pantilt, 	pantilt_update);
block_ondestroy(pantilt,	pantilt_destroy);
block_input(	pantilt, 	pan, 		'd', 	"Rotates the head by given value (param 1) radians between -3pi/2 to pi/2 (-270 degrees to +90 degrees)");
block_input(	pantilt, 	tilt, 		'd', 	"Tilts the head by given value (param 1) radians between -pi/4 ... pi/2 (-45 degrees to +90 degrees)");
block_output(	pantilt, 	pan_pwm, 	'i', 	"The output pwm for the pan servo (pwm measured in microseconds)");
block_output(	pantilt, 	tilt_pwm, 	'i',	"The output pwm for the tilt servo (pwm measured in microseconds)");
