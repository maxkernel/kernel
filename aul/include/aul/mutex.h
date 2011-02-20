#ifndef __AUL_THREAD_H
#define __AUL_THREAD_H

#include <pthread.h>
#include <time.h>
#include <aul/common.h>

#ifdef __cplusplus
extern "C" {
#endif

#define M_RECURSIVE			PTHREAD_MUTEX_RECURSIVE
#define M_NORMAL			PTHREAD_MUTEX_NORMAL

typedef pthread_mutex_t mutex_t;
#define mutex_lock(m)		pthread_mutex_lock(m)
#define mutex_unlock(m)		pthread_mutex_unlock(m)

static inline void mutex_init(mutex_t * mutex, int type)
{
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, type);
	pthread_mutex_init(mutex, &attr);
}

static inline mutex_t * mutex_new(int type)
{
	mutex_t * mutex = malloc0(sizeof(mutex_t));
	mutex_init(mutex, type);
	return mutex;
}


typedef pthread_cond_t		cond_t;
#define cond_signal(c)		pthread_cond_signal(c)
#define cond_broadcast(c)	pthread_cond_broadcast(c)

static inline void cond_init(cond_t * cond)
{
	pthread_condattr_t attr;
	pthread_condattr_init(&attr);
	pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	pthread_cond_init(cond, &attr);
}

static inline cond_t * cond_new()
{
	cond_t * cond = malloc0(sizeof(cond_t));
	cond_init(cond);
	return cond;
}

static inline void cond_wait(cond_t * cond, mutex_t * mutex, uint64_t nanoseconds)
{
	if (nanoseconds == 0LL)
	{
		pthread_cond_wait(cond, mutex);
	}
	else
	{
		struct timespec tm;
		clock_gettime(CLOCK_REALTIME, &tm);

		uint64_t nanos = ((uint64_t)tm.tv_nsec) + (nanoseconds % NANOS_PER_SECOND);
		if (nanos > NANOS_PER_SECOND)
		{
			tm.tv_sec += 1;
			nanos -= NANOS_PER_SECOND;
		}

		tm.tv_sec += nanoseconds / NANOS_PER_SECOND;
		tm.tv_nsec = nanos;

		pthread_cond_timedwait(cond, mutex, &tm);
	}
}


#ifdef __cplusplus
}
#endif
#endif
