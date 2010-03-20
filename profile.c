#include <inttypes.h>

#include "kernel.h"
#include "kernel-priv.h"

#ifdef EN_PROFILE

void profile_init()
{

}

void profile_addthreadrealtime(kthread_t * kth, uint64_t nanoseconds)
{
	//LOGK(LOG_INFO, "realtime of %s: %lld", kth->kobject.obj_name, nanoseconds);
}

void profile_addthreadcputime(kthread_t * kth, uint64_t nanoseconds)
{
	//LOGK(LOG_INFO, "cputime of %s: %lld", kth->kobject.obj_name, nanoseconds);
}

boolean profile_track(void * userdata)
{
	//LOG(LOG_INFO, "PROFILE TRACK");

	return TRUE;
}

#endif
