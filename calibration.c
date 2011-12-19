#include <stdio.h>
#include <sqlite3.h>
#include <string.h>
#include <limits.h>
#include <float.h>

#include <glib.h>

#include <aul/common.h>
#include "kernel.h"
#include "kernel-priv.h"

extern list_t calentries;
extern sqlite3 * database;

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
	sqlite3_stmt * stmt;
	calparam_t * c = NULL;
	const char * type = NULL, * value = NULL;
	
	string_t query = string_new("SELECT type, value FROM calibration WHERE name='%s' AND updated=( SELECT MAX(updated) FROM calibration WHERE name='%s' );", name, name);
	
	if (sqlite3_prepare_v2(database, query.string, -1, &stmt, NULL) != SQLITE_OK)
	{
		LOGK(LOG_ERR, "Could not compile SQL calibration query");
		goto done;
	}
	
	if (sqlite3_step(stmt) != SQLITE_ROW)
	{
		LOGK(LOG_DEBUG, "No calibration entry in database for %s", name);
		goto done;
	}

	type = (const char *)sqlite3_column_text(stmt, 0);
	value = (const char *)sqlite3_column_text(stmt, 1);
	
	if (type[0] != sig[0])
	{
		LOGK(LOG_WARN, "Calibration type mismatch. Type in database: %c, default value type: %c. Using default", type[0], sig[0]);
		goto done;
	}
	
done:
	if (value != NULL)
		c = cal_make(type[0], value);
	
	sqlite3_finalize(stmt);
	
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

	list_t * pos;
	calentry_t * cal = NULL;
	list_foreach(pos, &mod->calentries)
	{
		calentry_t * test = list_entry(pos, calentry_t, module_list);
		if (strcmp(name, test->name) == 0)
		{
			// found the entry
			cal = test;
			break;
		}
	}

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
		FREE(cal->preview);
	}
	cal->preview = pvalue;

	if (mod->calupdate != NULL)
	{
		mod->calpreview(name, cal->sig[0], pvalue, cal->active);
	}
}

void cal_merge(const char * comment)
{
	if (comment == NULL || strlen(comment) == 0)
	{
		comment = "(none)";
	}

	string_t query = string_blank();
	char value[25];

	list_t * pos;
	list_foreach(pos, &calentries)
	{
		calentry_t * cal = list_entry(pos, calentry_t, global_list);
		int changed = false;

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

			string_clear(&query);
			string_append(&query, "INSERT INTO calibration VALUES ('%s', '%c', '%s', DATETIME('NOW'), '%s');", cal->name, cal->sig[0], value, comment);
			if (sqlite3_exec(database, query.string, NULL, NULL, &err) != SQLITE_OK)
			{
				LOGK(LOG_ERR, "Could not compile SQL calibration insert query: %s", err);
			}

			if (cal->module->calupdate != NULL)
			{
				cal->module->calupdate(cal->name, cal->sig[0], cal->preview, cal->active);
			}
			FREE(cal->preview);
			cal->preview = NULL;
		}
	}
}

void cal_revert()
{
	list_t * pos;
	list_foreach(pos, &calentries)
	{
		calentry_t * cal = list_entry(pos, calentry_t, global_list);
		FREE(cal->preview);
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
	list_t * pos;
	list_foreach(pos, &calentries)
	{
		calentry_t * cal = list_entry(pos, calentry_t, global_list);
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
	}
}

int cal_compare(list_t * a, list_t * b)
{
	calentry_t * ca = list_entry(a, calentry_t, global_list);
	calentry_t * cb = list_entry(b, calentry_t, global_list);
	return strcmp(ca->name, cb->name);
}

void cal_freeparam(calparam_t * val)
{
	g_free(val);
}
