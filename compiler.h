/*
 * Tests requirements on the compiler.
 */


// 1) Make sure int and double are 4 and 8 bytes respectively
#if __SIZEOF_INT__ != 4
	#error Size of integer incompatible. Must be 4 bytes!
#endif
#if __SIZEOF_DOUBLE__ != 8
	#error Size of double incompatible. Must be 8 bytes!
#endif


// 2) Make sure we are little endian
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
	#error Archetecture incompatible. Must be little endian!
#endif

