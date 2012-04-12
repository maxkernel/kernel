#include <string.h>
#include <errno.h>

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
	free(sys->name);
	free(sys->sig);

	if (sys->desc != NULL)
	{
		free(sys->desc);
	}
}

syscall_t * syscall_new(const char * name, const char * sig, syscall_f func, const char * desc, exception_t ** err)
{
	// Sanity check
	{
		if (exception_check(err))
		{
			return NULL;
		}

		if (name == NULL || sig == NULL)
		{
			exception_set(err, EINVAL, "Bad arguments!");
			return NULL;
		}
	}

	syscall_t * syscall = kobj_new("Syscall", name, syscall_info, syscall_destroy, sizeof(syscall_t));

	syscall->name = strdup(name);
	syscall->sig = strdup(sig);
	syscall->func = func;
	syscall->desc = (desc == NULL)? NULL : strdup(desc);

	syscall->ffi = function_build(syscall->func, syscall->sig, err);
	if (syscall->ffi == NULL || exception_check(err))
	{
		return NULL;
	}

	hashtable_put(&syscalls, syscall->name, &syscall->global_entry);

	LOGK(LOG_DEBUG, "Registered syscall %s with sig %s", name, sig);
	return syscall;
}

bool asyscall_exec(const char * name, exception_t ** err, void * ret, void ** args)
{
	// Sanity check
	if (exception_check(err))
	{
		return false;
	}

	syscall_t * syscall = syscall_get(name);
	if (syscall == NULL)
	{
		exception_set(err, EINVAL, "Syscall %s doesn't exist!", name);
		return false;
	}
	
	function_call(syscall->ffi, ret, args);
	return true;
}

bool vsyscall_exec(const char * name, exception_t ** err, void * ret, va_list args)
{
	// Sanity check
	if (exception_check(err))
	{
		return false;
	}

	const syscall_t * syscall = syscall_get(name);
	if (syscall == NULL)
	{
		exception_set(err, EINVAL, "Syscall %s doesn't exist!", name);
		return false;
	}
	
	char array[SYSCALL_BUFFERMAX];
	ssize_t result = vserialize_2array_wheader((void **)array, SYSCALL_BUFFERMAX, method_params(syscall->sig), args);
	if (result == -1)
	{
		exception_set(err, ENOMEM, "Could not pack all arguments for syscall %s, consider increasing SYSCALL_BUFSIZE (currently %d)", name, SYSCALL_BUFFERMAX);
		return false;
	}

	return asyscall_exec(name, err, ret, (void **)array);
}

bool syscall_exec(const char * name, exception_t ** err, void * ret, ...)
{
	va_list args;
	va_start(args, ret);
	bool s = vsyscall_exec(name, err, ret, args);
	va_end(args);
	
	return s;
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

