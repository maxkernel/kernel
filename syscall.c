#include <string.h>
#include <ffi.h>
#include <glib.h>

#include <array.h>
#include <serialize.h>
#include <kernel.h>
#include <kernel-priv.h>

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
	FREE(sys->ptypes);
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
	

	// Prep the ffi cif
	{
		syscall->cif = malloc0(sizeof(ffi_cif));

		size_t params = method_numparams(method_params(syscall->sig));
		if (syscall->dynamic_data != NULL)
		{
			params += 1;
		}
		

		switch (method_returntype(syscall->sig))
		{
			case T_BOOLEAN:
			{
				// BOOL is 1 byte
				syscall->rtype = &ffi_type_uint8;
				break;
			}

			case T_INTEGER:
			{
				syscall->rtype = &ffi_type_sint32;
				break;
			}

			case T_DOUBLE:
			{
				syscall->rtype = &ffi_type_double;
				break;
			}

			case T_CHAR:
			{
				syscall->rtype = &ffi_type_schar;
				break;
			}

			case T_STRING:
			{
				syscall->rtype = &ffi_type_pointer;
				break;
			}

			case T_ARRAY_BOOLEAN:
			case T_ARRAY_INTEGER:
			case T_ARRAY_DOUBLE:
			case T_BUFFER:
			{
				syscall->rtype = &ffi_type_pointer;
				break;
			}

			case T_VOID:
			default:
			{
				syscall->rtype = &ffi_type_void;
				break;
			}
		}


		syscall->ptypes = malloc0(sizeof(ffi_type *) * (params + 1));	// Sentinel at end
		int index = 0;
		const char * param;

		if (syscall->dynamic_data != NULL)
		{
			syscall->ptypes[index++] = &ffi_type_pointer;
		}
		
		foreach_methodparam(method_params(syscall->sig), param)
		{
			switch (*param)
			{
				case T_BOOLEAN:
				{
					// BOOL is 1 byte
					syscall->ptypes[index++] = &ffi_type_uint8;
					break;
				}

				case T_INTEGER:
				{
					syscall->ptypes[index++] = &ffi_type_sint32;
					break;
				}

				case T_DOUBLE:
				{
					syscall->ptypes[index++] = &ffi_type_double;
					break;
				}

				case T_CHAR:
				{
					syscall->ptypes[index++] = &ffi_type_schar;
					break;
				}

				case T_STRING:
				{
					syscall->ptypes[index++] = &ffi_type_pointer;
					break;
				}

				case T_ARRAY_BOOLEAN:
				case T_ARRAY_INTEGER:
				case T_ARRAY_DOUBLE:
				case T_BUFFER:
				{
					syscall->ptypes[index++] = &ffi_type_pointer;
					break;
				}

				case T_VOID:
				default:
				{
					syscall->ptypes[index++] = &ffi_type_void;
					break;
				}
			}
		}
		
		
		// Do the preping
		ffi_prep_cif(syscall->cif, FFI_DEFAULT_ABI, index, syscall->rtype, syscall->ptypes);
	}
}

void * asyscall_exec(const char * name, void ** args)
{
	syscall_t * syscall = syscall_get(name);
	if (syscall == NULL)
	{
		LOGK(LOG_WARN, "Syscall %s doesn't exist!", name);
		return NULL;
	}
	
	void * rvalue = NULL;
	size_t rsize = 0;
	
	// Get return type size
	switch (method_returntype(syscall->sig))
	{
		case T_BOOLEAN:
		{
			rsize = sizeof(bool);
			break;
		}

		case T_INTEGER:
		{
			rsize = sizeof(int);
			break;
		}

		case T_DOUBLE:
		{
			rsize = sizeof(double);
			break;
		}

		case T_CHAR:
		{
			rsize = sizeof(char);
			break;
		}

		case T_STRING:
		{
			rsize = sizeof(char *);
			break;
		}

		case T_ARRAY_BOOLEAN:
		case T_ARRAY_INTEGER:
		case T_ARRAY_DOUBLE:
		case T_BUFFER:
		{
			rsize = sizeof(buffer_t);
			break;
		}

		case T_VOID:
		default:
		{
			rsize = 0;
			break;
		}
	}
	
	rvalue = (rsize > 0)? malloc0(rsize) : NULL;
	
	if (syscall->dynamic_data != NULL)
	{
		// Adjust the parameter list to include dynamic_data as first param
		void * nargs[method_numparams(method_params(syscall->sig)) + 1];
		nargs[0] = &syscall->dynamic_data;
		memcpy(&nargs[1], args, sizeof(void *) * method_numparams(method_params(syscall->sig)));

		ffi_call(syscall->cif, (void *)syscall->func, rvalue, nargs);
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
	
	char array[SYSCALL_BUFFERMAX];
	ssize_t result = vserialize_2array_wheader((void **)array, SYSCALL_BUFFERMAX, method_params(syscall->sig), args);
	if (result == -1)
	{
		LOGK(LOG_ERR, "Could not pack all arguments for syscall %s, consider increasing SYSCALL_BUFSIZE (currently %d)", name, SYSCALL_BUFFERMAX);
		return NULL;
	}

	void * ret = asyscall_exec(name, (void **)array);
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
	free(p);
}

bool syscall_exists(const char * name, const char * sig)
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

