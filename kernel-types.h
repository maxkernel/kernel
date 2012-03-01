#ifndef __KERNEL_TYPES_H
#define __KERNEL_TYPES_H

#define T_METHOD		'M'
#define T_ERROR			'E'
#define T_RETURN		'R'
#define T_VOID			'v'
#define T_BOOLEAN		'b'
#define T_INTEGER		'i'
#define T_DOUBLE		'd'
#define T_CHAR			'c'
#define T_STRING		's'
#define T_ARRAY_BOOLEAN	'B'
#define T_ARRAY_INTEGER	'I'
#define T_ARRAY_DOUBLE	'D'
#define T_BUFFER		'x'

#ifdef KERNEL
// Internal to kernel usage. Do not use!
#define T_POINTER		'p'
#endif

#if 0
#define S_VOID			"v"
#define S_BOOLEAN		"b"
#define S_INTEGER		"i"
#define S_DOUBLE		"d"
#define S_CHAR			"c"
#define S_STRING		"s"
#define S_ARRAY_BOOLEAN	"B"
#define S_ARRAY_INTEGER	"I"
#define S_ARRAY_DOUBLE	"D"
#define S_BUFFER		"x"
#endif

#endif
