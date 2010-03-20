#ifndef __SERIALIZE_H
#define __SERIALIZE_H

#include <stdarg.h>
#include <glib.h>

#include <buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __BOOLEAN
#define __BOOLEAN
typedef int boolean;
#endif

buffer_t serialize(const char * sig, ...);
buffer_t vserialize(const char * sig, va_list args);
buffer_t aserialize(const char * sig, void ** args);

boolean deserialize(const char * sig, void ** params, buffer_t buffer);

const char * method_params(const char * sig);
char method_returntype(const char * sig);
boolean method_isequal(const char * sig1, const char * sig2);

size_t param_arraysize(const char * sig);
void ** param_pack(const char * sig, ...);
void ** vparam_pack(const char * sig, va_list args);
void param_free(void ** array);

#ifdef __cplusplus
}
#endif
#endif
