#ifndef __AUL_CRC_H
#define __AUL_CRC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t crc_calc16(uint16_t startval, const void * data, size_t length);
uint32_t crc_calc32(uint32_t startval, const void * data, size_t length);

#ifdef __cplusplus
}
#endif
#endif
