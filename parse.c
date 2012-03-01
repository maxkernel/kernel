#include <errno.h>
#include <limits.h>
#include <ctype.h>

#include <aul/string.h>
#include <kernel.h>


int parse_int(const char * s, exception_t ** err)
{
	// Sanity check
	if (exception_check(err))
	{
		return 0;
	}

	errno = 0;
	long lvalue = 0;

	if (strprefix(s, "0x"))
	{
		// Parsing a hex number
		lvalue = strtol(s+2, NULL, 16);
	}
	else
	{
		// Parse the regular int
		lvalue = strtol(s, NULL, 10);
	}

	if (errno != 0)
	{
		exception_set(err, errno, "Could not parse '%s' to int!", s);
		return 0;
	}

	if (lvalue > INT_MAX || lvalue < INT_MIN)
	{
		exception_set(err, ERANGE, "Could not parse '%s' to int! Number to big or small!", s);
		return 0;
	}

	return (int)lvalue;
}

double parse_double(const char * s, exception_t ** err)
{
	// Sanity check
	if (exception_check(err))
	{
		return 0.0;
	}

	errno = 0;
	double value = strtod(s, NULL);

	if (errno != 0)
	{
		exception_set(err, errno, "Could not parse '%s' as double!", s);
		return 0.0;
	}

	return value;
}

bool parse_bool(const char * s, exception_t ** err)
{
	// Sanity check
	if (exception_check(err))
	{
		return false;
	}

	// Lowercase the string
	string_t str = string_new("%s", s);
	for (size_t i = 0; i < str.length; i++)
	{
		str.string[i] = tolower(str.string[i]);
	}

	// Check for true
	static const char * t[] = {"1", "true", "t", "yes"};
	for (size_t i = 0; i < nelems(t); i++)
	{
		if (strcmp(str.string, t[i]) == 0)
		{
			return true;
		}
	}

	// Check for false
	static const char * f[] = {"0", "false", "f", "no"};
	for (size_t i = 0; i < nelems(f); i++)
	{
		if (strcmp(str.string, f[i]) == 0)
		{
			return false;
		}
	}

	exception_set(err, EINVAL, "Cannot parse '%s' as bool!", s);
	return false;
}
