#include <stdio.h>
#include <sqlite.h>
#include <string.h>
#include <limits.h>
#include <float.h>

#include <aul/common.h>

#include "kernel.h"
#include "kernel-priv.h"

extern GSList * calentries;
extern sqlite * database;

static GRegex * bounds_regex = NULL;

static calparam_t * cal_make(const char type, const char * value)
{
	calparam_t * c = g_malloc(sizeof(calparam_t));
	c->type = type;
	strcpy(c->value, value);
	
	return c;
}

calparam_t * cal_getparam(const gchar * name, const gchar * sig)
{
	sqlite_vm * vm;
	calparam_t * c = NULL;
	
	GString * query = g_string_new("");
	g_string_printf(query, "SELECT type, value FROM calibration WHERE name='%s' AND updated=( SELECT MAX(updated) FROM calibration WHERE name='%s' );", name, name);
	
	gchar type;
	gchar * value = NULL;
	
	if (sqlite_compile(database, query->str, NULL, &vm, NULL) != SQLITE_OK)
	{
		LOGK(LOG_ERR, "Could not compile SQL calibration query");
		goto done;
	}
	
	const gchar ** coldata = NULL;
	if (sqlite_step(vm, NULL, &coldata, NULL) != SQLITE_ROW)
	{
		LOGK(LOG_DEBUG, "No calibration entry in database for %s", name);
		goto done;
	}
	
	type = coldata[0][0];
	value = (gchar *)coldata[1];
	
	if (type != sig[0])
	{
		LOGK(LOG_WARN, "Calibration type mismatch. Type in database: %c, default value type: %c. Using default", type, sig[0]);
		goto done;
	}
	
done:
	if (value != NULL)
		c = cal_make(type, value);
	
	g_string_free(query, TRUE);
	sqlite_finalize(vm, NULL);
	
	return c;
}

void cal_setparam(const char * module, const char * name, const char * value)
{
	module_t * mod = module_get(module);
	if (mod == NULL)
	{
		LOGK(LOG_WARN, "Could not set calibration parameter. Module %s doesn't exist", module);
		return;
	}

	calentry_t * cal = g_hash_table_lookup(mod->calentries, name);
	if (cal == NULL)
	{
		LOGK(LOG_WARN, "Could not set calibration parameter. Entry %s in module %s doesn't exist", name, module);
		return;
	}

	void * pvalue = NULL;
	switch (cal->sig[0])
	{
		#define __cal_setparam_elem(t1, t2, t3) \
			case t1: { \
				pvalue = g_malloc(sizeof(t2)); \
				*(int *)pvalue = t3; \
				break; }

		__cal_setparam_elem(T_INTEGER, int, atoi(value))
		__cal_setparam_elem(T_DOUBLE, double, strtod(value, NULL))

		default:
		{
			LOGK(LOG_WARN, "Could not set calibration parameter %s in module %s. Unknown signature '%c'", name, module, cal->sig[0]);
			return;
		}
	}

	//save value in preview variable
	if (cal->preview != NULL)
	{
		g_free(cal->preview);
	}
	cal->preview = pvalue;

	if (mod->calupdate != NULL)
	{
		mod->calupdate(name, cal->sig[0], pvalue, cal->active, TRUE);
	}
}

void cal_merge(const char * comment)
{
	if (comment == NULL || strlen(comment) == 0)
	{
		comment = "(none)";
	}

	GString * query = g_string_new("");
	char value[25];

	GSList * next = calentries;
	while (next != NULL)
	{
		calentry_t * cal = next->data;
		int changed = FALSE;

		if (cal->preview != NULL)
		{
			switch (cal->sig[0])
			{
				case T_INTEGER:
				{
					int preview = *(int *)cal->preview;
					int active = *(int *)cal->active;

					snprintf(value, sizeof(value), "%d", preview);
					changed = preview != active;

					break;
				}

				case T_DOUBLE:
				{
					double preview = *(double *)cal->preview;
					double active = *(double *)cal->active;

					snprintf(value, sizeof(value), "%lf", preview);
					changed = preview != active;

					break;
				}
			}
		}

		if (changed)
		{
			char * err;

			g_string_printf(query, "INSERT INTO calibration VALUES ('%s', '%c', '%s', DATETIME('NOW'), '%s');", cal->name, cal->sig[0], value, comment);
			if (sqlite_exec(database, query->str, NULL, NULL, &err) != SQLITE_OK)
			{
				LOGK(LOG_ERR, "Could not compile SQL calibration insert query: %s", err);
			}

			if (cal->module->calupdate != NULL)
			{
				cal->module->calupdate(cal->name, cal->sig[0], cal->preview, cal->active, FALSE);
			}
			g_free(cal->preview);
			cal->preview = NULL;
		}

		next = next->next;
	}
}

void cal_revert()
{
	GSList * next = calentries;
	while (next != NULL)
	{
		calentry_t * cal = next->data;
		FREE(cal->preview);

		next = next->next;
	}
}

static void cal_bounds(calentry_t * entry, char type, double * min_ptr, double * max_ptr)
{
	if (bounds_regex == NULL)
	{
		GError * err = NULL;
		bounds_regex = g_regex_new("^\\w([0-9\\.\\-]+):([0-9\\.\\-]+)$", G_REGEX_OPTIMIZE, 0, &err);

		if (err != NULL)
		{
			LOGK(LOG_FATAL, "Error creating cal sig RegEx: %s", err->message);
			g_error_free(err);
		}
	}

	double min = 0;
	double max = 0;

	GMatchInfo * info;
	g_regex_match(bounds_regex, entry->sig, 0, &info);
	if (!g_match_info_matches(info))
	{
		LOGK(LOG_WARN, "Malformed calibration signature: %s", entry->sig);
		goto done;
	}

	char * min_str = g_match_info_fetch(info, 1);
	char * max_str = g_match_info_fetch(info, 2);

	if (strcmp(min_str, "-") == 0)
	{
		min = (type == 'd')? DBL_MIN : INT_MIN;
	}
	else
	{
		min = strtod(min_str, NULL);
	}

	if (strcmp(max_str, "-") == 0)
	{
		max = (type == 'd')? DBL_MAX : INT_MAX;
	}
	else
	{
		max = strtod(max_str, NULL);
	}

	g_free(min_str);
	g_free(max_str);

done:
	g_match_info_free(info);
	*min_ptr = min;
	*max_ptr = max;
}

void cal_iterate(cal_itr_i itri, cal_itr_d itrd, void * userdata)
{
	GSList * next = calentries;
	while (next != NULL)
	{
		calentry_t * cal = next->data;
		double min, max;
		cal_bounds(cal, cal->sig[0], &min, &max);

		switch (cal->sig[0])
		{
			case T_INTEGER:
			{
				if (itri != NULL)
				{
					itri(userdata, cal->module->path, cal->name, cal->sig[0], cal->desc, *(int *)cal->active, (int)min, (int)max);
				}
				break;
			}

			case T_DOUBLE:
			{
				if (itrd != NULL)
				{
					itrd(userdata, cal->module->path, cal->name, cal->sig[0], cal->desc, *(double *)cal->active, min, max);
				}
				break;
			}
		}

		next = next->next;
	}
}

int cal_compare(const void * a, const void * b)
{
	const calentry_t * ca = a;
	const calentry_t * cb = b;
	return strcmp(ca->name, cb->name);
}

void cal_freeparam(calparam_t * val)
{
	g_free(val);
}

/*
double cal_map(const double dest_min, const double dest_center, const double dest_max, const double src_min, const double src_center, const double src_max, double value)
{
	value = CLAMP(value, src_min, src_max);

	if (value == src_center)
	{
		return dest_center;
	}
	else if (value > src_center)
	{
		return (value - src_center) / (src_max - src_center) * (dest_max - dest_center) + dest_center;
	}
	else
	{
		return (value - src_center) / (src_min - src_center) * (dest_min - dest_center) + dest_center;
	}

	return 0;
}
*/

