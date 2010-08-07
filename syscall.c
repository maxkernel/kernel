#include <string.h>
#include <ffi.h>

#include "kernel.h"
#include "kernel-priv.h"
#include "array.h"
#include "serialize.h"

extern GHashTable * syscalls;

static char * syscall_info(void * object)
{
	char * str = "[PLACEHOLDER SYSCALL INFO]";
	return strdup(str);
}

syscall_t * syscall_get(const char * name)
{
	return g_hash_table_lookup(syscalls, name);
}

void syscall_destroy(void * syscall)
{
	syscall_t * sys = syscall;
	FREE(sys->cif);
	FREES(sys->sig);
	FREES(sys->desc);
}

void syscall_reg(syscall_t * syscall)
{
	syscall->kobject.class_name = "Syscall";
	syscall->kobject.obj_name = syscall->name;
	syscall->kobject.info = syscall_info;
	syscall->kobject.destructor = syscall_destroy;

	g_hash_table_insert(syscalls, (void *)syscall->name, syscall);
	kobj_register((kobject_t *)syscall);
}

void * asyscall_exec(const char * name, void ** args)
{
	syscall_t * syscall = syscall_get(name);
	if (syscall == NULL)
	{
		LOGK(LOG_WARN, "Syscall %s doesn't exist!", name);
		return NULL;
	}
	
	if (syscall->cif == NULL)
	{
		//cif has not been built yet, do it now
		syscall->cif = g_malloc0(sizeof(ffi_cif));
		
		size_t param_len = strlen(method_params(syscall->sig));
		if (syscall->dynamic_data != NULL)
		{
			param_len += 1;
		}

		ffi_type * rtype; //return type
		ffi_type ** atype = g_malloc0(sizeof(ffi_type *) * param_len); //arg types
		
		switch (method_returntype(syscall->sig))
		{
			case T_BOOLEAN:
				// BOOL is 1 byte
				rtype = &ffi_type_uint8;	break;
			case T_INTEGER:
				rtype = &ffi_type_sint;		break;
			case T_DOUBLE:
				rtype = &ffi_type_double;	break;
			case T_CHAR:
				rtype = &ffi_type_schar;	break;
			case T_STRING:
				rtype = &ffi_type_pointer;	break;
			case T_ARRAY_BOOLEAN:
			case T_ARRAY_INTEGER:
			case T_ARRAY_DOUBLE:
			case T_BUFFER:
				rtype = &ffi_type_pointer;	break;
			default:
				rtype = &ffi_type_void;		break;
		}
		
		const char * sig = method_params(syscall->sig);
		int sigi = 0;

		if (syscall->dynamic_data != NULL)
		{
			atype[sigi++] = &ffi_type_pointer;
		}
		
		if (*sig != T_VOID)
		{
			while (*sig != '\0')
			{
				switch (*sig)
				{
					case T_BOOLEAN:
						// BOOL is 1 byte
						atype[sigi] = &ffi_type_uint8;		break;
					case T_INTEGER:
						atype[sigi] = &ffi_type_sint;		break;
					case T_DOUBLE:
						atype[sigi] = &ffi_type_double;		break;
					case T_CHAR:
						atype[sigi] = &ffi_type_schar;		break;
					case T_STRING:
						atype[sigi] = &ffi_type_pointer;	break;
					case T_ARRAY_BOOLEAN:
					case T_ARRAY_INTEGER:
					case T_ARRAY_DOUBLE:
					case T_BUFFER:
						atype[sigi] = &ffi_type_pointer;	break;
					default:
						atype[sigi] = &ffi_type_void;		break;
				}
			
				sig++;
				sigi++;
			}
		}
		
		
		//do the preping
		ffi_prep_cif(syscall->cif, FFI_DEFAULT_ABI, sigi, rtype, atype);
	}
	
	//LOGK(LOG_DEBUG, "Calling syscall %s", name);
	
	void * rvalue = NULL;
	size_t rsize = 0;
	
	//get return type size
	switch (syscall->sig[0])
	{
		case T_BOOLEAN:
			rsize = sizeof(bool);		break;
		case T_INTEGER:
			rsize = sizeof(int);		break;
		case T_DOUBLE:
			rsize = sizeof(double);		break;
		case T_CHAR:
			rsize = sizeof(char);		break;
		case T_STRING:
			rsize = sizeof(char *);		break;
		case T_ARRAY_BOOLEAN:
		case T_ARRAY_INTEGER:
		case T_ARRAY_DOUBLE:
		case T_BUFFER:
			rsize = sizeof(buffer_t);	break;
		default:
			rsize = 0;			break;
	}
	
	rvalue = (rsize > 0)? g_malloc0(rsize) : NULL;
	
	if (syscall->dynamic_data != NULL)
	{
		//adjust the parameter list to include dynamic_data as first param
		size_t plen = sizeof(void *) * (strlen(method_params(syscall->sig)));

		void ** nargs = g_malloc0(plen + sizeof(void *));
		nargs[0] = &syscall->dynamic_data;
		memcpy(nargs + 1, args, plen);

		ffi_call(syscall->cif, (void *)syscall->func, rvalue, nargs);

		g_free(nargs);
	}
	else
	{
		ffi_call(syscall->cif, (void *)syscall->func, rvalue, args);
	}

	return rvalue;
}

void * vsyscall_exec(const char * name, va_list args)
{
	const syscall_t * syscall = syscall_get(name);
	if (syscall == NULL)
	{
		LOGK(LOG_WARN, "Syscall %s doesn't exist!", name);
		return NULL;
	}
	
	//const char * sig = method_params(syscall->sig);

	/*
	GPtrArray * pargs = g_ptr_array_new();
	
	while (*sig != '\0')
	{
		switch (*sig)
		{
			#define __vsyscall_exec_element(t1, t2) \
				case t1: { \
					t2 * el = g_malloc(sizeof(t2)); \
					*el = (t2) va_arg(args, t2); \
					g_ptr_array_add(pargs, (void *) el); \
					break; }

			__vsyscall_exec_element(T_BOOLEAN, int)
			__vsyscall_exec_element(T_INTEGER, int)
			__vsyscall_exec_element(T_DOUBLE, double)
			__vsyscall_exec_element(T_CHAR, int)
			__vsyscall_exec_element(T_STRING, char *)
			__vsyscall_exec_element(T_ARRAY_BOOLEAN, array_t)
			__vsyscall_exec_element(T_ARRAY_INTEGER, array_t)
			__vsyscall_exec_element(T_ARRAY_DOUBLE, array_t)
			__vsyscall_exec_element(T_BUFFER, buffer_t)
			
			default:
				g_ptr_array_add(pargs, (void *) NULL);
				break;
		}
		
		sig++;
	}
	
	void * ret = asyscall_exec(name, pargs->pdata);
	
	size_t i=0;
	for (; i<pargs->len; i++)
		g_free(pargs->pdata[i]);
	g_ptr_array_free(pargs, true);
	*/
	
	void ** params = vparam_pack(method_params(syscall->sig), args);
	void * ret = asyscall_exec(name, params);
	param_free(params);

	return ret;
}

void * syscall_exec(const char * name, ...)
{
	va_list args;
	va_start(args, name);
	void * ret = vsyscall_exec(name, args);
	va_end(args);
	
	return ret;
}

void syscall_free(void * p)
{
	g_free(p);
}

bool syscall_exists(const gchar * name, const gchar * sig)
{
	const syscall_t * syscall = syscall_get(name);
	
	if (syscall == NULL)
		return false;
	
	if (sig != NULL && strlen(sig) > 0)
	{
		return method_isequal(sig, syscall->sig);
	}
	else
		return true;
}

