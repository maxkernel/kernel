#ifndef __AUL_PIPELINE_H
#define __AUL_PIPELINE_H

#include <aul/common.h>

#ifdef __cplusplus
extern "C" {
#endif


#define AUL_PIPELINE_SIGLEN		25

typedef struct
{
	const size_t objectsize;
	const char signature[AUL_PIPELINE_SIGLEN];
} pipenode_t;

#define __pipeline_blind(i, o) \
	typedef __pipeline_blind ## i (*__pipeline_blind ## o)(pipenode_t * pnode, ...)

typedef void (*__pipeline_blind0)(pipenode_t * pnode, ...);
__pipeline_blind( 0, 1); __pipeline_blind( 1, 2); __pipeline_blind( 2, 3); __pipeline_blind( 3, 4); __pipeline_blind( 4, 5);
__pipeline_blind( 5, 6); __pipeline_blind( 6, 7); __pipeline_blind( 7, 8); __pipeline_blind( 8, 9); __pipeline_blind( 9,10);
__pipeline_blind(10,11); __pipeline_blind(11,12); __pipeline_blind(12,13); __pipeline_blind(13,14); __pipeline_blind(14,15);
typedef __pipeline_blind15 (*pipeline_blind_f)(pipenode_t * pnode, ...);


typedef struct
{
	int i;
} pipeline_funcs_t;

typedef struct
{

	pipeline_funcs_t functions;
} pipeline_t;

#ifdef __cplusplus
}
#endif
#endif
