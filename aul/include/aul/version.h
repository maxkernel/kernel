#ifndef __AUL_VERSION_H
#define __AUL_VERSION_H

#include <inttypes.h>
#include <regex.h>

#include <aul/common.h>
#include <aul/parse.h>
#include <aul/string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUL_VERSION_TEXTSIZE	12

typedef uint64_t version_t;

#define version(major, minor, revision) (version_t)(((uint64_t)(major & 0xffff) << (6*8)) | ((uint64_t)(minor & 0xffff) << (4*8)) | (uint64_t)(revision & 0xffffff))

#define version_major(v)		((uint16_t)((v) >> (6*8)))
#define version_minor(v)		((uint16_t)((v) >> (4*8)))
#define version_revision(v)		((uint32_t)(v))

static inline string_t version_tostring(const version_t version)
{
    return string_new("%u.%u.%u", (unsigned int)version_major(version), (unsigned int)version_minor(version), (unsigned int)version_revision(version));
}

static inline version_t version_fromstring(const char * string)
{
	labels(end);

	// Sanity check
	{
		if unlikely(string == NULL)
		{
			return version(0,0,0);
		}
	}

	version_t version = version(0,0,0);

	regex_t pattern;
	memset(&pattern, 0, sizeof(regex_t));
	if (regcomp(&pattern, "^([0-9]+)\\.([0-9]+)\\.?([0-9]+)?$", REG_EXTENDED) != 0)
	{
		return false;
	}

	regmatch_t match[4];
	memset(match, 0, sizeof(regmatch_t) * 4);

	if (regexec(&pattern, string, 4, match, 0) != 0)
	{
		// Parsing failed
		goto end;
	}

	char major[AUL_VERSION_TEXTSIZE] = {0};
	char minor[AUL_VERSION_TEXTSIZE] = {0};
	char revision[AUL_VERSION_TEXTSIZE] = {0};

	memcpy(major, &string[match[1].rm_so], match[1].rm_eo - match[1].rm_so);
	memcpy(minor, &string[match[2].rm_so], match[2].rm_eo - match[2].rm_so);
	memcpy(revision, &string[match[3].rm_so], match[3].rm_eo - match[3].rm_so);

	version = version(parse_int(major, NULL), parse_int(minor, NULL), parse_int(revision, NULL));

end:
	regfree(&pattern);
	return version;
}

#ifdef __cplusplus
}
#endif
#endif
