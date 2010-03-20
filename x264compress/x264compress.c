#include <string.h>
#include <inttypes.h>
#include <x264.h>

#include <kernel.h>

DEF_BLOCK(compressor, x264c_new, "");
BLK_ONUPDATE(compressor, x264c_update);

#define ZERO(x) memset(&(x), 0, sizeof(x))

void * x264c_new()
{
	x264_param_t params;

	ZERO(params);
	x264_param_default(&params);

	x264_t * x264 = x264_encoder_open(&params);

	return x264;
}

void x264c_update(void * data)
{
	LOG(LOG_DEBUG, "x264 Update");

}

