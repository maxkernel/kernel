#include <stdio.h>
#include <sqlite3.h>
#include <string.h>
#include <limits.h>
#include <float.h>

#include <aul/common.h>
#include <aul/parse.h>

#include <kernel.h>
#include <kernel-priv.h>

#define SQL_SELECT_CALIBRATION		"SELECT value FROM calibration WHERE domain%s AND name='%s' AND sig='%c' AND updated=(SELECT MAX(updated) FROM calibration WHERE domain%s AND name='%s' AND sig='%c');"
#define SQL_INSERT_CALIBRATION		"INSERT INTO calibration VALUES (%s, '%s', '%c', '%s', DATETIME('NOW'), %s);"

extern sqlite3 * database;
extern calibration_t calibration;

static bool cal_getentry(const char * domain, const char * name, const char sig, char * tovalue, size_t tovalue_size)
{
	labels(done);

	bool success = false;
	sqlite3_stmt * stmt = NULL;
	const char * value = NULL;

	string_t domain_query = (domain == NULL)? string_new(" IS NULL") : string_new("='%s'", domain);
	string_t query = string_new(SQL_SELECT_CALIBRATION, domain_query.string, name, sig, domain_query.string, name, sig);

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
	string_t query = string_new(SQL_INSERT_CALIBRATION, domain_query.string, name, sig, value, comment_query.string);

	char * sql_err = NULL;
	if (sqlite3_exec(database, query.string, NULL, NULL, &sql_err) != SQLITE_OK)
	{
		LOGK(LOG_ERR, "Could not execute SQL calibration insert query: %s", sql_err);
		return false;
	}

	return true;
}

static bool cal_updatecache(calentry_t * entry)
{
	size_t wrote = 0;
	char cache[CAL_SIZE_CACHE] = {0};

	switch (entry->signature)
	{
		case T_INTEGER:
		{

			wrote = snprintf(cache, CAL_SIZE_CACHE, "%d", *(int *)entry->backing);
			break;
		}

		case T_DOUBLE:
		{
			wrote = snprintf(cache, CAL_SIZE_CACHE, "%f", *(double *)entry->backing);
			break;
		}

		default:
		{
			LOGK(LOG_ERR, "Unknown calibration entry type %s with sig %c", entry->name, entry->signature);
			return false;
		}
	}

	if (wrote >= (CAL_SIZE_CACHE - 1))
	{
		// Truncated!
		LOGK(LOG_ERR, "Could not cache calibration entry name %s in domain %s, too big!", entry->name, (entry->domain == NULL)? "(none)" : entry->domain);
		return false;
	}

	strcpy(entry->cache, cache);
	return true;
}

static bool cal_updatebacking(calentry_t * entry, const char * value)
{
	exception_t * e = NULL;

	switch (entry->signature)
	{
		case T_INTEGER:
		{
			int v = parse_int(value, &e);
			if (!exception_check(&e))	*(int *)entry->backing = v;
			break;
		}

		case T_DOUBLE:
		{
			double v = parse_double(value, &e);
			if (!exception_check(&e))	*(double *)entry->backing = v;
			break;
		}

		default:
		{
			LOGK(LOG_ERR, "Unknown calibration entry type %s with sig %c", entry->name, entry->signature);
			return false;
		}
	}

	if (exception_check(&e))
	{
		LOGK(LOG_ERR, "Could not set calibration entry %s: %s", entry->name, exception_message(e));
		exception_free(e);
		return false;
	}

	return true;
}

void cal_init()
{
	memset(&calibration, 0, sizeof(calibration_t));
	list_init(&calibration.entries);
	list_init(&calibration.previews);
	list_init(&calibration.modechanges);
}

void cal_sort()
{
	int cal_compare(list_t * a, list_t * b)
	{
		calentry_t * ca = list_entry(a, calentry_t, calibration_list);
		calentry_t * cb = list_entry(b, calentry_t, calibration_list);
		return strcmp(ca->name, cb->name);
	}

	list_sort(&calibration.entries, cal_compare);
}

void cal_doregister(const char * domain, const char * name, const char sig, const constraint_t constraints, const char * desc, void * backing, calpreview_f onpreview, void * onpreview_object)
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

	// Check to see if entry has already been registered
	// TODO IMPORTANT - FINISH ME

	// TODO - Add destroy function to entry!

	calentry_t * entry = malloc(sizeof(calentry_t));
	memset(entry, 0, sizeof(calentry_t));
	entry->domain = (domain == NULL || strlen(domain) == 0)? NULL : strdup(domain);
	entry->name = strdup(name);
	entry->signature = sig;
	entry->constraints = constraints;
	entry->description = strdup(desc);
	entry->backing = backing;
	entry->onpreview.callback = onpreview;
	entry->onpreview.object = onpreview_object;

	list_add(&calibration.entries, &entry->calibration_list);

	// Read entry from database, put it in cache
	if (!cal_getentry(domain, name, sig, entry->cache, CAL_SIZE_CACHE))
	{
		// Value not in database, read it from backing
		if (!cal_updatecache(entry))
		{
			return;
		}
	}

	// Write the calibration value back
	cal_updatebacking(entry, entry->cache);
}

void cal_onpreview(const char * domain, calgpreview_f callback, void * object)
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

calmode_t cal_getmode()
{
	return calibration.mode;
}

iterator_t cal_iterator()
{
	const void * cal_donext(const void * object, void ** itrobject)
	{
		unused(object);

		list_t * list = *itrobject = list_next((list_t *)*itrobject);
		return (list == &calibration.entries)? NULL : list_entry(list, calentry_t, calibration_list);
	}

	return iterator_new("calibration", cal_donext, NULL, NULL, &calibration.entries);

}

bool cal_next(iterator_t itr, const char ** domain, const char ** name, char * sig, const constraint_t ** constraints, const char ** desc, const char ** value)
{
	const calentry_t * entry = iterator_next(itr, "calibration");
	if (entry == NULL)
	{
		return false;
	}

	if (domain != NULL)			*domain			= entry->domain;
	if (name != NULL)			*name			= entry->name;
	if (sig != NULL)			*sig			= entry->signature;
	if (constraints != NULL)	*constraints	= &entry->constraints;
	if (desc != NULL)			*desc			= entry->description;
	if (value != NULL)			*value			= entry->cache;

	return true;
}

void cal_start()
{
	if (calibration.mode == calmode_calibrating)
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

			if (entry->checkpoint != NULL)
			{
				LOGK(LOG_WARN, "Calibration backup has already been performed for calibration %s in domain %s!", entry->name, entry->domain);
				continue;
			}

			entry->checkpoint = strdup(entry->cache);
		}
	}

	// Set the mode
	LOGK(LOG_INFO, "Entering calibration mode");
	calibration.mode = calmode_calibrating;

	// Notify the callbacks
	{
		list_t * pos = NULL;
		list_foreach(pos, &calibration.modechanges)
		{
			calmodechange_t * change = list_entry(pos, calmodechange_t, calibration_list);
			change->callback(change->object, calibration.mode, calstatus_ok);
		}
	}
}

const char * cal_preview(const char * domain, const char * name, const char * value, char * hint, size_t hint_length)
{
	if (calibration.mode == calmode_runtime)
	{
		LOGK(LOG_WARN, "Cannot preview calibration, not presently in calibration mode!");
		return NULL;
	}

	// Sanity check
	{
		if (name == NULL || value == NULL)
		{
			LOGK(LOG_ERR, "Bad arguments to cal_preview!");
			return NULL;
		}

		if (strlen(value) >= CAL_SIZE_CACHE)
		{
			LOGK(LOG_ERR, "Calibration value too long for entry %s in domain %s", name, (domain == NULL)? "(none)" : domain);
			return NULL;
		}
	}

	list_t * pos  = NULL;
	list_foreach(pos, &calibration.entries)
	{
		calentry_t * entry = list_entry(pos, calentry_t, calibration_list);
		if (
				((domain == NULL && entry->domain == NULL) || (domain != NULL && entry->domain != NULL && strcmp(domain, entry->domain) == 0)) &&
				(strcmp(name, entry->name) == 0)
		)
		{
			// We've found a match, update it

			if (!constraint_check(&entry->constraints, value, hint, hint_length))
			{
				// Failed the constraint check
				return entry->cache;
			}

			{
				// Update the backing
				if (!cal_updatebacking(entry, value))
				{
					// Could not update backing!
					return entry->cache;
				}

				// Call the callback
				if (entry->onpreview.callback != NULL)
				{
					entry->onpreview.callback(entry->onpreview.object, domain, name, entry->signature, entry->backing, hint, hint_length);
				}

				// Cache the backing (in case preview changed it)
				cal_updatecache(entry);
			}

			// Call the global preview cbs
			{
				list_t * pos = NULL;
				list_foreach(pos, &calibration.previews)
				{
					calpreview_t * preview = list_entry(pos, calpreview_t, calibration_list);
					if ((domain == NULL && preview->domain == NULL) || (domain != NULL && preview->domain != NULL && strcmp(domain, preview->domain) == 0))
					{
						preview->callback(preview->object, domain, name, entry->signature, entry->backing, hint, hint_length);
					}
				}
			}

			return entry->cache;
		}
	}

	LOGK(LOG_WARN, "Invalid calibration entry. Could not find calibration name %s in domain %s", name, (domain == NULL)? "(none)" : domain);
	return NULL;
}

void cal_commit(const char * comment)
{
	if (calibration.mode == calmode_runtime)
	{
		LOGK(LOG_WARN, "Cannot commit calibration, not presently in calibration mode!");
		return;
	}

	// Save the backings
	{
		list_t * pos = NULL;
		list_foreach(pos, &calibration.entries)
		{
			calentry_t * entry = list_entry(pos, calentry_t, calibration_list);
			if (strcmp(entry->cache, entry->checkpoint) != 0)
			{
				// Calibration entry has changed, save to database
				if (!cal_setentry(entry->domain, entry->name, entry->signature, entry->cache, comment))
				{
					LOGK(LOG_ERR, "Failed to commit calibration entry %s in domain %s", entry->name, (entry->domain == NULL)? "(none)" : entry->domain);
				}
			}

			free(entry->checkpoint);
			entry->checkpoint = NULL;
		}
	}

	// Set the mode
	LOGK(LOG_INFO, "Calibration committed with comment '%s'", (comment == NULL)? "(none)" : comment);
	calibration.mode = calmode_runtime;

	// Call the modechange functions
	{
		list_t * pos = NULL;
		list_foreach(pos, &calibration.modechanges)
		{
			calmodechange_t * change = list_entry(pos, calmodechange_t, calibration_list);
			change->callback(change->object, calibration.mode, calstatus_ok);
		}
	}
}

void cal_revert()
{
	if (calibration.mode == calmode_runtime)
	{
		LOGK(LOG_WARN, "Cannot revert calibration, not presently in calibration mode!");
		return;
	}

	// Revert the backings
	{
		list_t * pos = NULL;
		list_foreach(pos, &calibration.entries)
		{
			calentry_t * entry = list_entry(pos, calentry_t, calibration_list);
			if (strcmp(entry->cache, entry->checkpoint) != 0)
			{
				// Calibration entry has changed, revert it
				strcpy(entry->cache, entry->checkpoint);
			}

			free(entry->checkpoint);
			entry->checkpoint = NULL;
		}
	}

	// Set the mode
	LOGK(LOG_INFO, "Calibration reverted");
	calibration.mode = calmode_runtime;

	// Call the modechange functions
	{
		list_t * pos = NULL;
		list_foreach(pos, &calibration.modechanges)
		{
			calmodechange_t * change = list_entry(pos, calmodechange_t, calibration_list);
			change->callback(change->object, calibration.mode, calstatus_canceled);
		}
	}
}
