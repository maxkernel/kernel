#include <errno.h>
#include <ffi.h>

#include <aul/common.h>
#include <aul/exception.h>

#include <method.h>
#include <buffer.h>
#include <kernel.h>
#include <kernel-priv.h>


#define ffi_type(s) 		((sizeof(s) == 1)? FFI_TYPE_UINT8 : (sizeof(s) == 2)? FFI_TYPE_UINT16 : (sizeof(s) == 4)? FFI_TYPE_UINT32 : (sizeof(s) == 8)? FFI_TYPE_UINT64 : 0)
#define ffi_define(s, t)	{(s), (s), (t), NULL}

static ffi_type type_bool			= ffi_define(sizeof(bool),		ffi_type(bool));
static ffi_type type_int			= ffi_define(sizeof(int),		ffi_type(int));
static ffi_type type_double			= ffi_define(sizeof(double),	FFI_TYPE_DOUBLE);
static ffi_type type_char			= ffi_define(sizeof(char),		ffi_type(char));
static ffi_type type_string			= ffi_define(sizeof(void *),	FFI_TYPE_POINTER);
static ffi_type type_buffer			= ffi_define(sizeof(buffer_t),	ffi_type(buffer_t));
static ffi_type type_void 			= ffi_define(sizeof(void),		FFI_TYPE_VOID);
static ffi_type type_pointer		= ffi_define(sizeof(void *),	FFI_TYPE_POINTER);



ffi_t * function_build(void * function, const char * sig, exception_t ** err)
{
	// Sanity check
	if (exception_check(err))
	{
		return NULL;
	}

	ffi_t * ffi = malloc0(sizeof(ffi_t));
	ffi->function = function;
	ffi->cif = malloc0(sizeof(ffi_cif));
	ffi->atypes = malloc0(sizeof(ffi_type *) * (method_numparams(method_params(sig)) + 1)); // Sentinel at end

	int index = 0;
	const char * param;
	foreach_methodparam(method_params(sig), param)
	{
		switch (*param)
		{
			case T_VOID:		break;
			case T_POINTER:		ffi->atypes[index++] = &type_pointer;	break;
			case T_BOOLEAN:		ffi->atypes[index++] = &type_bool;		break;
			case T_INTEGER:		ffi->atypes[index++] = &type_int;		break;
			case T_DOUBLE:		ffi->atypes[index++] = &type_double;	break;
			case T_CHAR:		ffi->atypes[index++] = &type_char;		break;
			case T_STRING:		ffi->atypes[index++] = &type_string;	break;
			case T_ARRAY_BOOLEAN:
			case T_ARRAY_INTEGER:
			case T_ARRAY_DOUBLE:
			case T_BUFFER:		ffi->atypes[index++] = &type_buffer;	break;

			default:
			{
				exception_set(err, EINVAL, "Unknown value (%c) in signature '%s'!", *param, sig);
				function_free(ffi);
				return NULL;
			}
		}
	}

	switch (method_returntype(sig))
	{
		case T_VOID:		ffi->rtype = &type_void;		break;
		case T_POINTER:		ffi->rtype = &type_pointer;		break;
		case T_BOOLEAN:		ffi->rtype = &type_bool;		break;
		case T_INTEGER:		ffi->rtype = &type_int;			break;
		case T_DOUBLE:		ffi->rtype = &type_double;		break;
		case T_CHAR:		ffi->rtype = &type_char;		break;
		case T_STRING:		ffi->rtype = &type_string;		break;
		case T_ARRAY_BOOLEAN:
		case T_ARRAY_INTEGER:
		case T_ARRAY_DOUBLE:
		case T_BUFFER:		ffi->rtype = &type_buffer;		break;

		default:
		{
			exception_set(err, EINVAL, "Unknown return value (%c) in signature '%s'!", method_returntype(sig), sig);
			function_free(ffi);
			return NULL;
		}
	}

	// Prep the cif
	ffi_status status = ffi_prep_cif((ffi_cif *)ffi->cif, FFI_DEFAULT_ABI, method_numparams(method_params(sig)), (ffi_type *)ffi->rtype, (ffi_type **)ffi->atypes);
	if (status != FFI_OK)
	{
		exception_set(err, EFAULT, "Internal FFI Error(%d)! Could not create function with sig '%s'", (int)status, sig);
		function_free(ffi);
		return NULL;
	}

	return ffi;
}

void function_free(ffi_t * ffi)
{
	free(ffi->atypes);
	free(ffi->cif);
	free(ffi);
}

void function_call(ffi_t * ffi, void * ret, void ** args)
{
	ffi_call((ffi_cif *)ffi->cif, ffi->function, ret, args);
}
