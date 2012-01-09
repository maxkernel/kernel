#include <math.h>

#include <aul/common.h>

#include <map.h>
#include <kernel.h>

MOD_VERSION("1.0");
MOD_AUTHOR("Andrew Klofas - <andrew@maxkernel.com>");
MOD_DESCRIPTION("Helper module for use in applications where the robot has a camera turret.");
MOD_DEPENDENCY("map");
MOD_INIT(model_init);
MOD_DESTROY(model_destroy);

BLK_INPUT(STATIC, pan, "d", "Rotates the head by given value (param 1) radians between -3pi/2 to pi/2 (-270 degrees to +90 degrees)");
BLK_INPUT(STATIC, tilt, "d", "Tilts the head by given value (param 1) radians between -pi/4 ... pi/2 (-45 degrees to +90 degrees)");
BLK_OUTPUT(STATIC, pan_pwm, "i");
BLK_OUTPUT(STATIC, tilt_pwm, "i");
BLK_ONUPDATE(STATIC, model_update);


// Default calibration settings
int pan_min		= 5000,		pan_forward		= 8000,		pan_backward	= 6000,			pan_max		= 9000;
int tilt_min	= 5000,		tilt_center		= 7000,		tilt_max		= 9000;

CAL_UPDATE(model_updatecal);
CAL_PREVIEW(model_previewcal);
CAL_PARAM(	pan_min,		"i0:15000",		"Head Pan Minimum (left -270 deg)"		);
CAL_PARAM(	pan_forward,	"i0:15000",		"Head Pan Center Forward"				);
CAL_PARAM(	pan_backward,	"i0:15000",		"Head Pan Center Backward"				);
CAL_PARAM(	pan_max,		"i0:15000",		"Head Pan Maximum (right +90 deg)"		);
CAL_PARAM(	tilt_min,		"i0:15000",		"Head Tilt Minimum (down -45 deg)"		);
CAL_PARAM(	tilt_center,	"i0:15000",		"Head Tilt Center (level forward)"		);
CAL_PARAM(	tilt_max,		"i0:15000",		"Head Tilt Maximum (up +90 deg)"		);


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

void model_previewcal(const char * name, const char type, void * newvalue, void * target)
{
	mode_calibration = true;

	if		(strprefix(name, "pan"))		pwm_pan		= *(int *)newvalue;
	else if	(strprefix(name, "tilt"))		pwm_tilt	= *(int *)newvalue;
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

void model_init()
{
	pwm_pan = pan_forward;
	pwm_tilt = tilt_center;

	setmaps();
}

void model_destroy()
{
	map_destroy(map_pan);
	map_destroy(map_tilt);
}
