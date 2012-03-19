#ifndef __KERNEL_TYPES_H
#define __KERNEL_TYPES_H

// TODO - should these be moved to console/libmax?
#define T_METHOD		'M'		// Used in messages (rpc)
#define T_ERROR			'E'		// Used in messages (rpc)
#define T_RETURN		'R'		// Used in messages (rpc)


#define T_VOID			'v'		// Valid in any signature
#define T_BOOLEAN		'b'		// Valid in any signature
#define T_INTEGER		'i'		// Valid in any signature
#define T_DOUBLE		'd'		// Valid in any signature
#define T_CHAR			'c'		// Valid in any signature.
#define T_STRING		's'		// Valid in any signature. In rpc, the mapping is (const char *). As input, the maximum size is deined as ?? TODO - find the size!

#define T_ARRAY_BOOLEAN	'B'		// Only valid in input/output signatures
#define T_ARRAY_INTEGER	'I'		// Only valid in input/output signatures
#define T_ARRAY_DOUBLE	'D'		// Only valid in input/output signatures
#define T_BUFFER		'x'		// Only valid in input/output signatures

#define T_POINTER		'p'		// Never valid. Do not use! (Internal to kernel usage. Do not use!)

#endif
