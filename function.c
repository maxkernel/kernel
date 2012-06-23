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

static ffi_type type_bool			= ffi_define(sizeof(bool),			ffi_type(bool));
static ffi_type type_int			= ffi_define(sizeof(int),			ffi_type(int));
static ffi_type type_double			= ffi_define(sizeof(double),		FFI_TYPE_DOUBLE);
static ffi_type type_char			= ffi_define(sizeof(char),			ffi_type(char));
static ffi_type type_string			= ffi_define(sizeof(void *),		FFI_TYPE_POINTER);
static ffi_type type_buffer			= ffi_define(sizeof(buffer_t *),	FFI_TYPE_POINTER);
static ffi_type type_void 			= ffi_define(sizeof(void),			FFI_TYPE_VOID);
static ffi_type type_pointer		= ffi_define(sizeof(void *),		FFI_TYPE_POINTER);

static bool ffi_prep(const char * sig, ffi_type ** ret, ffi_type *** args, exception_t ** err)
{
	size_t index = 0;

	const char * param = NULL;
	method_foreachparam(param, method_params(sig))
	{
		switch (*param)
		{
			case T_VOID:		break;
			case T_POINTER:		(*args)[index++] = &type_pointer;	break;
			case T_BOOLEAN:		(*args)[index++] = &type_bool;		break;
			case T_INTEGER:		(*args)[index++] = &type_int;		break;
			case T_DOUBLE:		(*args)[index++] = &type_double;	break;
			case T_CHAR:		(*args)[index++] = &type_char;		break;
			case T_STRING:		(*args)[index++] = &type_string;	break;
			case T_ARRAY_BOOLEAN:
			case T_ARRAY_INTEGER:
			case T_ARRAY_DOUBLE:
			case T_BUFFER:		(*args)[index++] = &type_buffer;	break;

			default:
			{
				exception_set(err, EINVAL, "Unknown value (%c) in signature '%s'!", *param, sig);
				return false;
			}
		}
	}

	switch (method_returntype(sig))
	{
		case T_VOID:		*ret = &type_void;		break;
		case T_POINTER:		*ret = &type_pointer;	break;
		case T_BOOLEAN:		*ret = &type_bool;		break;
		case T_INTEGER:		*ret = &type_int;		break;
		case T_DOUBLE:		*ret = &type_double;	break;
		case T_CHAR:		*ret = &type_char;		break;
		case T_STRING:		*ret = &type_string;	break;
		case T_ARRAY_BOOLEAN:
		case T_ARRAY_INTEGER:
		case T_ARRAY_DOUBLE:
		case T_BUFFER:		*ret = &type_buffer;	break;

		default:
		{
			exception_set(err, EINVAL, "Unknown return value (%c) in signature '%s'!", method_returntype(sig), sig);
			return false;
		}
	}

	return true;
}

ffi_function_t * function_build(void * function, const char * sig, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(function == NULL || sig == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}

	size_t asize = sizeof(ffi_type *) * (method_numparams(method_params(sig)) + 1); // Sentinel at end

	ffi_function_t * ffi = malloc(sizeof(ffi_function_t));
	memset(ffi, 0, sizeof(ffi_function_t));
	ffi->function = function;
	ffi->cif = malloc(sizeof(ffi_cif));
	ffi->atypes = malloc(asize);
	memset(ffi->cif, 0, sizeof(ffi_cif));
	memset(ffi->atypes, 0, asize);

	// Prep sig --> ffi
	if (!ffi_prep(sig, (ffi_type **)&ffi->rtype, (ffi_type ***)&ffi->atypes, err))
	{
		function_free(ffi);
		return NULL;
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

void function_free(ffi_function_t * ffi)
{
	// Sanity check
	{
		if unlikely(ffi == NULL)
		{
			return;
		}
	}

	free(ffi->atypes);
	free(ffi->cif);
	free(ffi);
}

void function_call(ffi_function_t * ffi, void * ret, void ** args)
{
	// Sanity check
	{
		if unlikely(ffi == NULL)
		{
			return;
		}
	}

	ffi_call((ffi_cif *)ffi->cif, ffi->function, ret, args);
}


static void closure_callback(ffi_cif * cif, void * ret, void ** args, void * closure)
{
	unused(cif);

	ffi_closure_t * c = closure;
	c->callback(ret, (const void **)args, c->userdata);
}

ffi_closure_t * closure_build(void * function, closure_f callback, const char * sig, void * userdata, exception_t ** err)
{
	// Sanity check
	{
		if unlikely(exception_check(err))
		{
			return NULL;
		}

		if unlikely(function == NULL || callback == NULL || sig == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}

	size_t asize = sizeof(ffi_type *) * (method_numparams(method_params(sig)) + 1); // Sentinel at end

	ffi_closure_t * ci = malloc(sizeof(ffi_closure_t));
	memset(ci, 0, sizeof(ffi_closure_t));
	ci->callback = callback;
	ci->closure = ffi_closure_alloc(sizeof(ffi_closure), function);
	ci->cif = malloc(sizeof(ffi_cif));
	ci->atypes = malloc(asize);
	ci->userdata = userdata;
	memset(ci->cif, 0, sizeof(ffi_cif));
	memset(ci->atypes, 0, asize);

	// Prep sig --> ffi
	if (!ffi_prep(sig, (ffi_type **)&ci->rtype, (ffi_type ***)&ci->atypes, err))
	{
		closure_free(ci);
		return NULL;
	}

	// Prep the cif
	ffi_status status = ffi_prep_cif((ffi_cif *)ci->cif, FFI_DEFAULT_ABI, method_numparams(method_params(sig)), (ffi_type *)ci->rtype, (ffi_type **)ci->atypes);
	if (status != FFI_OK)
	{
		exception_set(err, EFAULT, "Internal FFI Error(%d)! Could not prep closure with sig '%s'", (int)status, sig);
		closure_free(ci);
		return NULL;
	}

	// Prep the closure
	status = ffi_prep_closure_loc((ffi_closure *)ci->closure, (ffi_cif *)ci->cif, closure_callback, ci, *(void **)function);
	if (status != FFI_OK)
	{
		exception_set(err, EFAULT, "Internal FFI Error(%d)! Could not create closure with sig '%s'", (int)status, sig);
		closure_free(ci);
		return NULL;
	}

	return ci;
}

void closure_free(ffi_closure_t * closure)
{
	// Sanity check
	{
		if unlikely(closure == NULL)
		{
			return;
		}
	}

	free(closure->atypes);
	free(closure->cif);
	ffi_closure_free(closure->closure);
	free(closure);
}
