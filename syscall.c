#include <string.h>

#include <array.h>
#include <serialize.h>
#include <kernel.h>
#include <kernel-priv.h>

extern hashtable_t syscalls;

static char * syscall_info(void * object)
{
	char * str = "[PLACEHOLDER SYSCALL INFO]";
	return strdup(str);
}

syscall_t * syscall_get(const char * name)
{
	hashentry_t * entry = hashtable_get(&syscalls, name);
	return (entry == NULL)? NULL : hashtable_entry(entry, syscall_t, global_entry);
}

void syscall_destroy(void * syscall)
{
	syscall_t * sys = syscall;
	hashtable_remove(&sys->global_entry);

	function_free(sys->ffi);
	FREES(sys->sig);
	FREES(sys->desc);
}

void syscall_reg(syscall_t * syscall)
{
	syscall->kobject.class_name = "Syscall";
	syscall->kobject.obj_name = syscall->name;
	syscall->kobject.info = syscall_info;
	syscall->kobject.destructor = syscall_destroy;
	kobj_register(&syscall->kobject);

	exception_t * err = NULL;
	syscall->ffi = function_build(syscall->func, syscall->sig, &err);
	if (exception_check(&err))
	{
		LOGK(LOG_FATAL, "Could not create syscall %s: Code %d %s", syscall->name, err->code, err->message);
		// Will exit
	}

	hashtable_put(&syscalls, syscall->name, &syscall->global_entry);
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
			rsize = sizeof(int);
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
	
	function_call(syscall->ffi, rvalue, args);

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

