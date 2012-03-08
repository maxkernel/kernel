#ifndef __AUL_BASE64_H
#define __AUL_BASE64_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define base64_strlen(data) ((((data) + 2) / 3) * 4)
size_t base64_encode(const uint8_t * in, size_t inlen, char * out, size_t outlen);
size_t base64_decode(const char * in, size_t inlen, uint8_t * out, size_t outlen);
bool base64_isvalid(const char v);

#ifdef __cplusplus
}
#endif
#endif
