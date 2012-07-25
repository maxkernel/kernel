#include <map.h>
#include <kernel.h>


typedef struct
{
	map_t * map;
	double n1, z, p1;
	double cal_preview;
} unit_t;

static void unit_setmaps(unit_t * unit)
{
	if (unit->map != NULL)
	{
		map_destroy(unit->map);
		unit->map = NULL;
	}

	double from[] = { -1.0, 0.0, 1.0 };
	double to[] = { unit->n1, unit->z, unit->p1 };
	unit->map = map_newfromarray(MAP_LINEAR, from, to, 3);
}

static void unit_calpreview(void * object, const char * domain, const char * name, const char sig, void * backing, char * hint, size_t hint_length)
{
	unit_t * unit = object;
	unit->cal_preview = *(double *)backing;
}

static void unit_calmodechange(void * object, calmode_t mode, calstatus_t status)
{
	unit_t * unit = object;

	if (mode == calmode_runtime)
	{
		// Just completed calibration, reset the maps
		unit_setmaps(unit);
	}
}

static void * unit_new(const char * domain, double n1, const char * n1_desc, double z, const char * z_desc, double p1, const char * p1_desc)
{
	unit_t * unit = malloc(sizeof(unit_t));
	memset(unit, 0, sizeof(unit_t));
	unit->map = NULL;
	unit->n1 = n1;
	unit->z = z;
	unit->p1 = p1;
	unit->cal_preview = z;

	cal_register(domain,	"negative",	&unit->n1,	'd',	n1_desc,	unit_calpreview,	unit);
	cal_register(domain,	"zero",		&unit->z,	'd',	z_desc,		unit_calpreview,	unit);
	cal_register(domain,	"positive",	&unit->p1,	'd',	p1_desc,	unit_calpreview,	unit);
	cal_onmodechange(unit_calmodechange, unit);

	unit_setmaps(unit);

	return NULL;
}

static void unit_update(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	unit_t * unit = object;

	if (cal_getmode() == calmode_runtime)
	{
		const double * in = input(value);
		double out = map_tovalue(unit->map, (in == NULL)? 0.0 : *in);
		output(value, &out);
	}
	else
	{
		output(value, &unit->cal_preview);
	}
}

static void unit_destroy(void * object)
{
	// Sanity check
	{
		if unlikely(object == NULL)
		{
			return;
		}
	}

	unit_t * unit = object;
	map_destroy(unit->map);
	free(unit);
}


module_name("Calibration helper module");
module_version(1, 0, 0);
module_author("Andrew Klofas <andrew@maxkernel.com>");
module_description("Exposes the MaxKernel calibration subsystem to the siticher layer as Blocks");
module_dependency("map");

define_block(unit, "-1 to 1 Calibration Mapping", unit_new, "sdsdsds", "(1) Cal domain (2) -1.0 Default map (3) -1.0 Cal Desc (4) 0.0 Default map (5) 0.0 Cal Desc (6) 1.0 Default map (7) 1.0 Cal Desc");
block_onupdate(unit, unit_update);
block_ondestroy(unit, unit_destroy);
block_input(unit, value, T_DOUBLE, "Input value (-1.0 to 1.0)");
block_output(unit, value, T_DOUBLE, "Output value (determined by calibration)");

