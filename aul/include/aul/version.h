#ifndef __AUL_VERSION_H
#define __AUL_VERSION_H

#include <inttypes.h>

#include <aul/string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t version_t;

#define version(major, minor, revision) (((uint64_t)(major & 0xffff) << (6*8)) | ((uint64_t)(minor & 0xffff) << (4*8)) | (uint64_t)(revision & 0xffffff))

#define version_major(v)		((uint16_t)((v) >> (6*8)))
#define version_minor(v)		((uint16_t)((v) >> (4*8)))
#define version_revision(v)		((uint32_t)(v))

static inline string_t version_tostring(const version_t version)
{
    return string_new("%u.%u.%u", (unsigned int)version_major(version), (unsigned int)version_minor(version), (unsigned int)version_revision(version));
}

#ifdef __cplusplus
}
#endif
#endif
