#include <math.h>

#include <aul/common.h>

#include <map.h>
#include <kernel.h>


// Default calibration settings
int pan_min		= 1500,		pan_forward		= 1800,		pan_backward	= 1600,			pan_max		= 1900;
int tilt_min	= 1400,		tilt_center		= 1500,		tilt_max		= 1700;


static int pwm_pan = 0, pwm_tilt = 0;
static map_t * map_pan = NULL, * map_tilt = NULL;

static void setmaps()
{
	{ // Pan map
		if (map_pan != NULL) map_destroy(map_pan);
		double pan_from[] = {-M_PI, 0.0, M_PI/2.0, M_PI/2.0+0.0001, M_PI};
		double pan_to[] = {pan_backward, pan_forward, pan_max, pan_min, pan_backward};
		map_pan = map_newfromarray(MAP_LINEAR, pan_from, pan_to, 5);
	}

	{ // Tilt map
		if (map_tilt != NULL) map_destroy(map_tilt);
		double tilt_from[] = {-M_PI/4.0, 0.0, M_PI/2.0};
		double tilt_to[] = {tilt_min, tilt_center, tilt_max};
		map_tilt = map_newfromarray(MAP_LINEAR, tilt_from, tilt_to, 3);
	}
}

static void model_calpreview(void * object, const char * domain, const char * name, const char sig, void * backing, char * hint, size_t hint_length)
{
	if		(strprefix(name, "pan"))		pwm_pan		= *(int *)backing;
	else if	(strprefix(name, "tilt"))		pwm_tilt	= *(int *)backing;
}

static void model_calmodechange(void * object, calmode_t mode, calstatus_t status)
{
	if (mode == calmode_runtime)
	{
		// Just completed calibration, reset the maps
		setmaps();
	}
}

void model_update(void * obj)
{
	if (cal_getmode() == calmode_runtime)
	{
		// Not in calibration mode, read inputs and apply maps

		const double * value_pan = INPUT(pan);
		const double * value_tilt = INPUT(tilt);

		if (value_pan != NULL)
		{
			double pan = *value_pan;
			while (pan > M_PI) { pan -= 2.0*M_PI; }
			while (pan < -M_PI) { pan += 2.0*M_PI; }

			pwm_pan = (int)map_tovalue(map_pan, pan);
		}

		if (value_tilt != NULL)
			pwm_tilt = (int)map_tovalue(map_tilt, clamp(*value_tilt, -M_PI/4.0, M_PI/2.0));
	}

	OUTPUT(pan_pwm, &pwm_pan);
	OUTPUT(tilt_pwm, &pwm_tilt);
}

bool model_init()
{
	cal_register(	NULL,	pan_min,		'i',		"[0:15000] Head Pan Minimum (left -270 deg)",	model_calpreview,	NULL);
	cal_register(	NULL,	pan_forward,	'i',		"[0:15000] Head Pan Center Forward",			model_calpreview,	NULL);
	cal_register(	NULL,	pan_backward,	'i',		"[0:15000] Head Pan Center Backward",			model_calpreview,	NULL);
	cal_register(	NULL,	pan_max,		'i',		"[0:15000] Head Pan Maximum (right +90 deg)",	model_calpreview,	NULL);
	cal_register(	NULL,	tilt_min,		'i',		"[0:15000] Head Tilt Minimum (down -45 deg)",	model_calpreview,	NULL);
	cal_register(	NULL,	tilt_center,	'i',		"[0:15000] Head Tilt Center (level forward)",	model_calpreview,	NULL);
	cal_register(	NULL,	tilt_max,		'i',		"[0:15000] Head Tilt Maximum (up +90 deg)",		model_calpreview,	NULL);
	cal_onmodechange(model_calmodechange,	NULL);

	pwm_pan = pan_forward;
	pwm_tilt = tilt_center;

	setmaps();
	return true;
}

void model_destroy()
{
	map_destroy(map_pan);
	map_destroy(map_tilt);
}


module_name("Look Model");
module_version(1,0,0);
module_author("Andrew Klofas - andrew@maxkernel.com");
module_description("Helper module for use in applications where the robot has a camera turret.");
module_dependency("map");
module_oninitialize(model_init);
module_ondestroy(model_destroy);

block_input(	static, 	pan, 		'd', 	"Rotates the head by given value (param 1) radians between -3pi/2 to pi/2 (-270 degrees to +90 degrees)");
block_input(	static, 	tilt, 		'd', 	"Tilts the head by given value (param 1) radians between -pi/4 ... pi/2 (-45 degrees to +90 degrees)");
block_output(	static, 	pan_pwm, 	'i', 	"The output pwm for the pan servo (pwm measured in microseconds)");
block_output(	static, 	tilt_pwm, 	'i',	"The output pwm for the tilt servo (pwm measured in microseconds)");
block_onupdate(	static, 	model_update);
