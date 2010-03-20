#include <glib.h>
#include <string.h>
#include <kernel.h>

#include "map.h"

MOD_VERSION("0.9");
MOD_AUTHOR("Andrew Klofas <aklofas@gmail.com>");
MOD_DESCRIPTION("A helper module that allows you to map linear values to a nonlinear curve. Useful for calibration maps");


#define MAX_MATCHES		50

static GRegex * map_regex = NULL;

static double map_linear(map_t * map, double tomap, size_t ia, size_t ib)
{
	double diff_src = map->src_array[ib] - map->src_array[ia];
	double diff_dest = map->dest_array[ib] - map->dest_array[ia];

	//no delta on source
	if (diff_src == 0.0)
	{
		return (map->dest_array[ia]+map->dest_array[ib])/2.0;
	}

	//no delta on dest
	if (diff_dest == 0.0)
	{
		return (map->dest_array[ia]);
	}

	double percent_src = (tomap - map->src_array[ia])/(diff_src);
	double result = (percent_src * diff_dest) + map->dest_array[ia];

	return result;
}

static map_f map_getmode(char mode)
{
	switch (mode)
	{
		case MAP_LINEAR:
		default:
			return map_linear;
	}
}

static void map_sort(map_t * map)
{
	size_t mpos = map->array_size - 1;

	//quick and simple bubble sort
	while (mpos > 0)
	{
		size_t i = 0;
		for (; i<mpos; i++)
		{
			if (map->src_array[i] > map->src_array[i+1])
			{
				//swap the indexes
				double tmp;

				tmp = map->src_array[i];
				map->src_array[i] = map->src_array[i+1];
				map->src_array[i+1] = tmp;

				tmp = map->dest_array[i];
				map->dest_array[i] = map->dest_array[i+1];
				map->dest_array[i+1] = tmp;
			}
		}

		mpos--;
	}
}

static map_t * map_new(map_f mapping_function, double * from, double * to, size_t length)
{
	size_t sz = sizeof(double) * length;
	double * src = g_slice_alloc0(sz);
	double * dest = g_slice_alloc0(sz);
	memcpy(src, from, sz);
	memcpy(dest, to, sz);

	map_t * map = g_slice_alloc0(sizeof(map_t));
	map->src_array = src;
	map->dest_array = dest;
	map->array_size = length;
	map->mapping_function = mapping_function;

	map_sort(map);

	return map;
}

void map_destroy(map_t * map)
{
	size_t sz = sizeof(double) * map->array_size;
	g_slice_free1(sz, map->src_array);
	g_slice_free1(sz, map->dest_array);
	g_slice_free1(sizeof(map_t), map);
}

map_t * map_newfromstring(char mode, const char * string)
{
	if (map_regex == NULL)
	{
		GError * err = NULL;
		map_regex = g_regex_new("\\s*\\(\\s*(-?[[:digit:]\\.]+)\\s*,\\s*(-?[[:digit:]\\.]+)\\s*\\)\\s*,?", G_REGEX_OPTIMIZE, 0, &err);

		if (err != NULL)
		{
			LOG(LOG_ERR, "Error creating Map RegEx: %s", err->message);
			g_error_free(err);
			return NULL;
		}
	}

	GMatchInfo * info;
	g_regex_match(map_regex, string, 0, &info);

	if (!g_match_info_matches(info))
	{
		LOG(LOG_ERR, "Invalid format. Could not parse into map: %s", string);
		return NULL;
	}

	size_t num = 1;
	double * a = g_malloc0(sizeof(double) * MAX_MATCHES);
	double * b = g_malloc0(sizeof(double) * MAX_MATCHES);

	a[0] = strtod(g_match_info_fetch(info, 1), NULL);
	b[0] = strtod(g_match_info_fetch(info, 2), NULL);

	while (g_match_info_next(info, NULL))
	{
		a[num] = strtod(g_match_info_fetch(info, 1), NULL);
		b[num] = strtod(g_match_info_fetch(info, 2), NULL);
		num++;

		if (num == MAX_MATCHES)
		{
			LOG(LOG_WARN, "Reached the MAX_MATCHES limit (%d) for string %s", MAX_MATCHES, string);
			break;
		}
	}

	map_t * map = map_newfromarray(mode, a, b, num);

	g_free(a);
	g_free(b);

	return map;
}

map_t * map_newfromarray(char mode, double * from, double * to, size_t length)
{
	return map_new(map_getmode(mode), from, to, length);
}

map_t * map_reverse(map_t * map)
{
	size_t sz = sizeof(double) * map->array_size;
	double * src = g_malloc(sz);
	double * dest = g_malloc(sz);

	int i=0;
	for (; i<map->array_size; i++)
	{
		src[i] = map->src_array[map->array_size - i - 1];
	}
	memcpy(dest, map->dest_array, sz);

	return map_new(map->mapping_function, src, dest, map->array_size);
}

double map_tovalue(map_t * map, double tomap)
{
	if (map->array_size == 0)
		return 0.0;

	if (tomap <= map->src_array[0])
		return map->dest_array[0];

	size_t index = 1;
	for (; index<=map->array_size; index++)
	{
		if (tomap >= map->src_array[index-1] && tomap <= map->src_array[index])
		{
			return map->mapping_function(map, tomap, index-1, index);
		}
	}

	return map->dest_array[map->array_size - 1];
}

