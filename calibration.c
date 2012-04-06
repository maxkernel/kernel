#include <stdio.h>
#include <sqlite3.h>
#include <string.h>
#include <limits.h>
#include <float.h>

#include <glib.h>

#include <aul/common.h>
#include "kernel.h"
#include "kernel-priv.h"

extern sqlite3 * database;
extern calibration_t calibration;

static bool cal_getentry(const char * domain, const char * name, const char sig, char * tovalue, size_t tovalue_size)
{
	LABELS(done);

	bool success = false;
	sqlite3_stmt * stmt;
	const char * value = NULL;

	string_t domain_query = (domain == NULL)? string_new("domain='%s' AND ", domain) : string_blank();
	string_t query = string_new("SELECT value FROM calibration WHERE %sname='%s' AND sig='%c' AND updated=(SELECT MAX(updated) FROM calibration WHERE %sname='%s' AND sig='%c');", domain_query.string, name, sig, domain_query.string, name, sig);

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

	value = (const char *)sqlite3_column_text(stmt, 0);
	if (strlen(value) >= tovalue_size)
	{
		LOGK(LOG_WARN, "Saved calibration size too big for entry %s in domain %s", name, domain);
		goto done;
	}

	strcpy(tovalue, value);
	success = true;

done:
	sqlite3_finalize(stmt);
	return success;
}

static bool cal_setentry(const char * domain, const char * name, const char sig, const char * value, const char * comment)
{
	string_t domain_query = (domain == NULL)? string_new("NULL") : string_new("'%s'", domain);
	string_t comment_query = (comment == NULL)? string_new("NULL") : string_new("'%s'", comment);
	string_t query = string_append(&query, "INSERT INTO calibration VALUES (%s, '%s', '%c', '%s', DATETIME('NOW'), %s);", domain_query.string, name, sig, value, comment_query.string);

	char * sql_err = NULL;
	if (sqlite3_exec(database, query.string, NULL, NULL, &sql_err) != SQLITE_OK)
	{
		LOGK(LOG_ERR, "Could not execute SQL calibration insert query: %s", sql_err);
		return false;
	}

	return true;
}

static bool cal_setcache(calentry_t * entry, void * value)
{
	size_t wrote = 0;
	char cache[CAL_SIZE_CACHE] = {0};

	switch (entry->sig)
	{
		case T_INTEGER:
		{

			wrote = snprintf(cache, CAL_SIZE_CACHE, "%d", *(int *)value);
			break;
		}

		case T_DOUBLE:
		{
			wrote = snprintf(entry->cache, CAL_SIZE_CACHE, "%f", *(double *)value);
			break;
		}

		default:
		{
			return false;
		}
	}

	if (wrote >= (CAL_SIZE_CACHE - 1))
	{
		// Truncated!
		return false;
	}

	memcpy(entry->cache, cache, CAL_SIZE_CACHE);
	return true;
}

void cal_init()
{
	memset(&calibration, 0, sizeof(calibration_t));
	LIST_INIT(&calibration->entries);
	LIST_INIT(&calibration->previews);
	LIST_INIT(&calibration->modechanges);
}

void cal_register(const char * domain, const char * name, const char sig, const char * constraints, void * backing, calpreview_f onpreview, void * onpreview_object)
{
	// Sanity check
	{
		if (name == NULL || backing == NULL)
		{
			LOGK(LOG_ERR, "Bad arguments to cal_register!");
			return;
		}

		switch (sig)
		{
			case T_DOUBLE:
			case T_INTEGER:
			case T_BOOLEAN:
				break;

			default:
			{
				LOGK(LOG_ERR, "Invalid calibration type '%c' to entry %s in domain %s", sig, name, (domain == NULL)? "(none)" : domain);
				return;
			}
		}
	}

	calentry_t * entry = malloc(sizeof(calentry_t));
	memset(entry, 0, sizeof(calentry_t));
	entry->domain = (domain == NULL)? NULL : strdup(domain);
	entry->name = strdup(name);
	entry->sig = sig;
	entry->constraints = (constraints == NULL)? NULL : strdup(constraints);
	entry->backing = backing;
	entry->onpreview.callback = onpreview;
	entry->onpreview.object = onpreview_object;

	list_add(&calibration.entries, &entry->calibration_list);

	// Read entry from database, put it in cache
	if (!cal_getentry(domain, name, sig, entry->cache, CAL_SIZE_CACHE))
	{
		// Value not in database, read it from backing
		if (!cal_setcache(entry, backing))
		{
			LOGK(LOG_ERR, "Could not read calibration entry name %d with sig %c!", name, sig);
			return;
		}
	}

	// Write the calibration value back
	{
		exception_t * e = NULL;

		switch (entry->sig)
		{
			case T_INTEGER:
			{
				int value = parse_int(entry->cache, &e);
				if (!exception_check(&e))	*(int *)entry->backing = value;
				break;
			}

			case T_DOUBLE:
			{
				double value = parse_double(entry->cache, &e);
				if (!exception_check(&e))	*(double *)entry->backing = value;
				break;
			}

			default:
			{
				LOGK(LOG_ERR, "Unknown calibration entry type %s with sig %c", name, sig);
				return;
			}
		}

		if (exception_check(&e))
		{
			LOGK(LOG_ERR, "Could not set calibration entry %s: %s", name, e->message);
			exception_free(e);
		}
	}
}

void cal_onpreview(const char * domain, calpreview_f callback, void * object)
{
	// Sanity check
	{
		if (callback == NULL)
		{
			return;
		}
	}

	calpreview_t * onpreview = malloc(sizeof(calpreview_t));
	if (onpreview == NULL)
	{
		LOGK(LOG_ERR, "Out of heap memory!");
		return;
	}

	memset(onpreview, 0, sizeof(calpreview_t));
	onpreview->domain = (domain == NULL)? NULL : strdup(domain);
	onpreview->callback = callback;
	onpreview->object = object;

	list_add(&calibration.previews, &onpreview->calibration_list);
}

void cal_onmodechange(calmodechange_f callback, void * object)
{
	// Sanity check
	{
		if (callback == NULL)
		{
			return;
		}
	}

	calmodechange_t * onmodechange = malloc(sizeof(calmodechange_t));
	if (onmodechange == NULL)
	{
		LOGK(LOG_ERR, "Out of heap memory!");
		return;
	}

	memset(onmodechange, 0, sizeof(calmodechange_t));
	onmodechange->callback = callback;
	onmodechange->object = object;

	list_add(&calibration.modechanges, &onmodechange->calibration_list);
}

void cal_startcalibration()
{
	if (calibration.mode == calmode_enter)
	{
		LOGK(LOG_WARN, "Already in calibration mode!");
		return;
	}

	// Backup all existing calibration
	{
		list_t * pos = NULL;
		list_foreach(pos, &calibration.entries)
		{
			calentry_t * entry = list_entry(pos, calentry_t, calibration_list);

			if (entry->edit_backup != NULL)
			{
				LOGK(LOG_WARN, "Calibration backup has already been performed for calibration %s in domain %s!", entry->name, entry->domain);
				continue;
			}

			entry->edit_backup = strdup(entry->cache);
		}
	}

	// Set the mode
	calibration.mode = calmode_enter;

	// Notify the callbacks
	{
		list_t * pos = NULL;
		list_foreach(pos, &calibration.modechanges)
		{
			calmodechange_t * change = list_entry(pos, calmodechange_t, calibration_list);
			if (change->callback != NULL)
			{
				change->callback(change->object, calibration.mode, calstatus_ok);
			}
		}
	}
}

#if 0
//static GRegex * bounds_regex = NULL;

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

void cal_setparam(const char * modname, const char * name, const char * value)
{
	// TODO - DO CALIBRATION PROCEDURE!
	/*
	const module_t * module = module_lookup(modname);
	if (module == NULL)
	{
		LOGK(LOG_WARN, "Could not set calibration parameter. Module %s doesn't exist", modname);
		return;
	}

	list_t * pos;
	const calentry_t * cal = NULL;
	list_foreach(pos, &module->calentries)
	{
		const calentry_t * test = list_entry(pos, calentry_t, module_list);
		if (strcmp(name, test->name) == 0)
		{
			// found the entry
			cal = test;
			break;
		}
	}

	if (cal == NULL)
	{
		LOGK(LOG_WARN, "Could not set calibration parameter. Entry %s in module %s doesn't exist", name, modname);
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

		__cal_setparam_elem(T_INTEGER, int, parse_int(value, NULL))
		__cal_setparam_elem(T_DOUBLE, double, parse_double(value, NULL))

		default:
		{
			LOGK(LOG_WARN, "Could not set calibration parameter %s in module %s. Unknown signature '%c'", name, modname, cal->sig[0]);
			return;
		}
	}

	//save value in preview variable
	if (cal->preview != NULL)
	{
		FREE(cal->preview);
	}
	cal->preview = pvalue;

	if (module->calupdate != NULL)
	{
		module->calpreview(name, cal->sig[0], pvalue, cal->active);
	}
	*/
}

void cal_merge(const char * comment)
{
	/*
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
	*/
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

/*
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
		min = parse_double(min_str, NULL);
	}

	if (strcmp(max_str, "-") == 0)
	{
		max = (type == 'd')? DBL_MAX : INT_MAX;
	}
	else
	{
		max = parse_double(max_str, NULL);
	}

	g_free(min_str);
	g_free(max_str);

done:
	g_match_info_free(info);
	*min_ptr = min;
	*max_ptr = max;
}
*/

void cal_iterate(cal_itr_i itri, cal_itr_d itrd, void * userdata)
{
	/*
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
	*/
}

void cal_freeparam(calparam_t * val)
{
	g_free(val);
}
#endif
