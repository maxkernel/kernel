#ifndef __MAP_H
#define __MAP_H

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAP_LINEAR	'L'

struct __map_t;
typedef double (*map_f)(struct __map_t * map, const double tomap, size_t ia, size_t ib);

typedef struct __map_t
{
	double * src_array;
	double * dest_array;
	size_t array_size;
	map_f mapping_function;
} map_t;

map_t * map_newfromstring(char mode, const char * string);
map_t * map_newfromarray(char mode, double * from, double * to, size_t length);
void map_destroy(map_t * map);

double map_tovalue(map_t * map, double tomap);

//helper utils
map_t * map_reverse(map_t * map);

#ifdef __cplusplus
}
#endif
#endif
