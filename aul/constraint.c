#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <regex.h>

#include <aul/constraint.h>
#include <aul/parse.h>

typedef struct
{
	double min, max;
	double step;
} crange_t;

static bool crange_new(const char * constraint, void * object)
{
	LABELS(end);

	regex_t pattern = {0};
	bool success = false;
	if (regcomp(&pattern, "^([0-9\\.]+):([0-9\\.]+:)?([0-9\\.]+)$", REG_EXTENDED) != 0)
	{
		return false;
	}

	regmatch_t match[4];
	memset(match, 0, sizeof(regmatch_t) * 4);

	if (regexec(&pattern, constraint, 4, match, 0) != 0)
	{
		// Parsing failed
		goto end;
	}

	// Parsing worked, pull out the fields
	{
		char min[AUL_CONSTRAINT_TEXTSIZE] = {0};
		char step[AUL_CONSTRAINT_TEXTSIZE] = {0};
		char max[AUL_CONSTRAINT_TEXTSIZE] = {0};

		memcpy(min, &constraint[match[1].rm_so], match[1].rm_eo - match[1].rm_so);
		memcpy(step, &constraint[match[2].rm_so], match[2].rm_eo - match[2].rm_so);
		memcpy(max, &constraint[match[3].rm_so], match[3].rm_eo - match[3].rm_so);

		char * colon = strchr(step, ':');
		if (colon != NULL)	*colon = '\0';

		exception_t * e = NULL;
		crange_t * range = object;
		range->min = parse_double(min, &e);
		range->step = parse_double(step, &e);
		range->max = parse_double(max, &e);

		if (exception_check(&e))
		{
			// Could not parse something or other :-/
			exception_free(e);
			goto end;
		}

		success = true;
	}

end:
	regfree(&pattern);
	return success;
}

static bool crange_test(const void * object, const char * value, char * hint, size_t hint_length)
{
	const crange_t * range = object;
	double v = 0;

	// Parse value
	{
		exception_t * e = NULL;
		v = parse_double(value, &e);

		if (exception_check(&e))
		{
			snprintf(hint, hint_length, "Must be a number!");

			exception_free(e);
			return false;
		}
	}

	if (v < range->min)
	{
		snprintf(hint, hint_length, "Minimum value is %f", range->min);
		return false;
	}

	if (v > range->max)
	{
		snprintf(hint, hint_length, "Maximum value is %f", range->max);
		return false;
	}

	return true;
}

static bool crange_step(const void * object, double * step)
{
	const crange_t * range = object;

	if (range->step != 0)
	{
		*step = range->step;
		return true;
	}

	return false;
}

constraint_t constraint_new(const char * constraint, exception_t ** err)
{
	constraint_t c;
	memset(&c, 0, sizeof(constraint_t));

	// Sanity check
	{
		if (strlen(constraint) >= AUL_CONSTRAINT_TEXTSIZE)
		{
			exception_set(err, E2BIG, "Constraint string too long (AUL_CONSTRAINT_TEXTSIZE = %d)", AUL_CONSTRAINT_TEXTSIZE);
			return c;
		}
	}

	// Test range
	{
		if (crange_new(constraint, (void *)c.privdata))
		{
			c.cbs.test = crange_test;
			c.cbs.step = crange_step;
			return c;
		}
	}

	// No dice, did not match any constraint types
	memset(&c, 0, sizeof(constraint_t));
	exception_set(err, EINVAL, "Could not match any constraint patterns for constraint '%s'", constraint);
	return c;
}

bool constraint_check(const constraint_t * constraint, const char * value, char * hint, size_t hint_length)
{
	// Sanity check
	{
		if (constraint == NULL)
		{
			// Be conservative, default pass all
			return true;
		}
	}

	if (constraint->cbs.test == NULL)
	{
		// No test function
		return true;
	}

	return constraint->cbs.test((const void *)constraint->privdata, value, hint, hint_length);
}

bool constraint_getstep(const constraint_t * constraint, double * step)
{
	// Sanity check
	{
		if (constraint == NULL || constraint->cbs.step == NULL || step == NULL)
		{
			return false;
		}
	}

	return constraint->cbs.step((const void *)constraint->privdata, step);
}

constraint_t constraint_parse(const char * string, exception_t ** err)
{
	LABELS(noparse);

	#define return_empty() \
		({ constraint_t c; memset(&c, 0, sizeof(constraint_t)); return c;})

	regex_t pattern = {0};
	if (regcomp(&pattern, "^[ ]*\\[([^]]+)\\]", REG_EXTENDED) != 0)
	{
		exception_set(err, EFAULT, "Bad constraint_parse regex!");
		return_empty();
	}

	regmatch_t match[2];
	memset(match, 0, sizeof(regmatch_t) * 2);

	if (regexec(&pattern, string, 2, match, 0) != 0)
	{
		// Parsing failed
		goto noparse;
	}

	if ((match[1].rm_eo - match[1].rm_so) >= AUL_CONSTRAINT_TEXTSIZE)
	{
		exception_set(err, E2BIG, "Constraint string too long (AUL_CONSTRAINT_TEXTSIZE = %d)", AUL_CONSTRAINT_TEXTSIZE);
		goto noparse;
	}

	// Successful
	{
		// Copy constraint string into new buffer
		char buf[AUL_CONSTRAINT_TEXTSIZE] = {0};
		memcpy(buf, &string[match[1].rm_so], match[1].rm_eo - match[1].rm_so);

		return constraint_new(buf, err);
	}

noparse:
	regfree(&pattern);
	return_empty();
}

const char * constraint_parsepast(const char * string)
{
	LABELS(end);

	regex_t pattern = {0};
	size_t index = 0;
	if (regcomp(&pattern, "^[ ]*\\[[^]]+\\][ ]*", REG_EXTENDED) != 0)
	{
		return string;
	}

	regmatch_t match[1];
	memset(match, 0, sizeof(regmatch_t) * 1);

	if (regexec(&pattern, string, 1, match, 0) != 0)
	{
		goto end;
	}

	index = match[0].rm_eo;

end:
	regfree(&pattern);
	return &string[index];
}
