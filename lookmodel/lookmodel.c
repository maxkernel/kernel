#include <math.h>

#include <aul/common.h>

#include <map.h>
#include <kernel.h>


// Default calibration settings
int pan_min		= 5000,		pan_forward		= 8000,		pan_backward	= 6000,			pan_max		= 9000;
int tilt_min	= 5000,		tilt_center		= 7000,		tilt_max		= 9000;


static bool mode_calibration = false;
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

void model_updatecal(const char * name, const char type, void * newvalue, void * target)
{
	if (type != T_INTEGER)
	{
		LOG(LOG_WARN, "Unknown calibration entry: %s", name);
		return;
	}

	memcpy(target, newvalue, sizeof(int));
	setmaps();

	mode_calibration = false;
}

void model_calpreview(void * object, const char * domain, const char * name, const char sig, void * backing, char * hint, size_t hint_length)
{
	/*
	mode_calibration = true;

	if		(strprefix(name, "pan"))		pwm_pan		= *(int *)newvalue;
	else if	(strprefix(name, "tilt"))		pwm_tilt	= *(int *)newvalue;
	*/
}

void model_calmodechange(void * object, calmode_t mode, calstatus_t status)
{

}

void model_update(void * obj)
{
	if (mode_calibration == false)
	{
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
			pwm_tilt = (int)map_tovalue(map_tilt, CLAMP(*value_tilt, -M_PI/4.0, M_PI/2.0));
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
