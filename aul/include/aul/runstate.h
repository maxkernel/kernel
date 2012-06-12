#ifndef __AUL_RUNSTATE_H
#define __AUL_RUNSTATE_H


#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	runstate_unknown		= 0,
	runstate_initialize		= 1,
	runstate_setup			= 2,
	runstate_runtime		= 3,
	runstate_cleanup		= 4,
} runstate_t;



#ifdef __cplusplus
}
#endif
#endif
