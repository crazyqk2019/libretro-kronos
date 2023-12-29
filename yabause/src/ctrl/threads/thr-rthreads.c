/*  src/thr-rthreads.c: Thread functions for Libretro
    Copyright 2019 barbudreadmon. Based on code by Andrew Church, Lawrence Sebald, Theo Berkau, devMiyax, and FCare

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Some parts of this file don't use rthreads because rthreads doesn't cover all required functionalities

#if !defined(ARCH_IS_LINUX) && !defined(ARCH_IS_MACOSX)
	#include <windows.h>
#else
	#include <sched.h>
	#include <unistd.h>
	#if defined(ARCH_IS_MACOSX)
		#include <dispatch/dispatch.h>
	#else
		#include <semaphore.h>
	#endif
#endif

#include "core.h"
#include "threads.h"
#include "rthreads/rthreads.h"
#include <stdlib.h>

struct thd_s {
	int running;
	sthread_t *thd;
	slock_t *mutex;
	scond_t *cond;
};

typedef struct YabEventQueue_rthreads
{
	void **buffer;
	int capacity;
	int size;
	int in;
	int out;
	slock_t *mutex;
	scond_t *cond_full;
	scond_t *cond_empty;
} YabEventQueue_rthreads;

typedef struct YabMutex_rthreads
{
  slock_t *mutex;
} YabMutex_rthreads;

static struct thd_s thread_handle[YAB_NUM_THREADS];

#if !defined(ARCH_IS_LINUX) && !defined(ARCH_IS_MACOSX)
#ifdef HAVE_THREAD_STORAGE
static sthread_tls_t hnd_key;
static int hnd_key_once = 0;
#endif
#endif

//////////////////////////////////////////////////////////////////////////////

int YabThreadStart(unsigned int id, void* (*func)(void *), void *arg)
{
#if !defined(ARCH_IS_LINUX) && !defined(ARCH_IS_MACOSX)
#ifdef HAVE_THREAD_STORAGE
	if (hnd_key_once == 0)
	{
		if(sthread_tls_create(&hnd_key));
			hnd_key_once = 1;
	}
#endif
#endif

	if ((thread_handle[id].thd = sthread_create((void *)func, arg)) == NULL)
	{
		perror("CreateThread");
		return -1;
	}
	if ((thread_handle[id].mutex = slock_new()) == NULL)
	{
		perror("CreateThread");
		return -1;
	}
	if ((thread_handle[id].cond = scond_new()) == NULL)
	{
		perror("CreateThread");
		return -1;
	}

	thread_handle[id].running = 1;

	return 0;
}

void YabThreadWait(unsigned int id)
{
	if (thread_handle[id].running == 0)
		return;

	sthread_join(thread_handle[id].thd);
	thread_handle[id].thd = NULL;
	thread_handle[id].running = 0;
}

void YabThreadYield(void)
{
#if !defined(ARCH_IS_LINUX) && !defined(ARCH_IS_MACOSX)
	SleepEx(0, 0);
#else
	sched_yield();
#endif
}

void YabThreadWake(unsigned int id)
{
	if (thread_handle[id].running == 0)
		return;

	scond_signal(thread_handle[id].cond);
}

void YabAddEventQueue( YabEventQueue * queue_t, void* evcode )
{
	YabEventQueue_rthreads * queue = (YabEventQueue_rthreads*)queue_t;
	slock_lock(queue->mutex);
	while (queue->size == queue->capacity)
		scond_wait(queue->cond_full, queue->mutex);
	queue->buffer[queue->in] = evcode;
	++ queue->size;
	++ queue->in;
	queue->in %= queue->capacity;
	slock_unlock(queue->mutex);
	scond_broadcast(queue->cond_empty);
}

u32 YabThreadUSleep( unsigned int stime )
{
#if !defined(ARCH_IS_LINUX) && !defined(ARCH_IS_MACOSX)
	SleepEx(stime/1000, 0);
	return stime%1000;
#else
	usleep(stime);
	return 0;
#endif
}

void YabThreadSetCurrentThreadAffinityMask(int mask)
{
}

void* YabWaitEventQueue( YabEventQueue * queue_t )
{
	void* value;
	YabEventQueue_rthreads * queue = (YabEventQueue_rthreads*)queue_t;
	slock_lock(queue->mutex);
	while (queue->size == 0)
		scond_wait(queue->cond_empty, queue->mutex);
	value = queue->buffer[queue->out];
	-- queue->size;
	++ queue->out;
	queue->out %= queue->capacity;
	slock_unlock(queue->mutex);
	scond_broadcast(queue->cond_full);
	return value;
}

int YaGetQueueSize(YabEventQueue * queue_t)
{
	int size = 0;
	YabEventQueue_rthreads * queue = (YabEventQueue_rthreads*)queue_t;
	slock_lock(queue->mutex);
	size = queue->size;
	slock_unlock(queue->mutex);
	return size;
}

YabEventQueue * YabThreadCreateQueue( int qsize )
{
	YabEventQueue_rthreads * p = (YabEventQueue_rthreads*)malloc(sizeof(YabEventQueue_rthreads));
	p->buffer = (void**)malloc( sizeof(void*)* qsize);
	p->capacity = qsize;
	p->size = 0;
	p->in = 0;
	p->out = 0;
	p->mutex = slock_new();
	p->cond_full = scond_new();
	p->cond_empty = scond_new();
	return (YabEventQueue *)p;
}

void YabThreadDestoryQueue( YabEventQueue * queue_t )
{
	slock_t *mutex;
	YabEventQueue_rthreads * queue = (YabEventQueue_rthreads*)queue_t;
	mutex = queue->mutex;
	slock_lock(mutex);
	while (queue->size == queue->capacity)
		scond_wait(queue->cond_full, queue->mutex);
	free(queue->buffer);
	free(queue);
	slock_unlock(mutex);
}

void YabThreadLock( YabMutex * mtx )
{
	YabMutex_rthreads * pmtx;
	pmtx = (YabMutex_rthreads *)mtx;
	slock_lock(pmtx->mutex);
}

void YabThreadUnLock( YabMutex * mtx )
{
	YabMutex_rthreads * pmtx;
	pmtx = (YabMutex_rthreads *)mtx;
	slock_unlock(pmtx->mutex);
}

void YabThreadFreeMutex( YabMutex * mtx )
{
	if( mtx != NULL )
	{
		YabMutex_rthreads * pmtx;
		pmtx = (YabMutex_rthreads *)mtx;
		slock_free(pmtx->mutex);
		free(pmtx);
	}
}

YabMutex * YabThreadCreateMutex()
{
	YabMutex_rthreads * mtx = (YabMutex_rthreads *)malloc(sizeof(YabMutex_rthreads));
	mtx->mutex = slock_new();
	return (YabMutex *)mtx;
}

//////////////////////////////////////////////////////////////////////////////

typedef struct YabSem_rthreads
{
#if defined(ARCH_IS_LINUX)
	sem_t sem;
#elif defined(ARCH_IS_MACOSX)
	dispatch_semaphore_t sem;
#else
	HANDLE sem;
#endif
} YabSem_rthreads;

void YabSemPost( YabSem * mtx )
{
	YabSem_rthreads * pmtx;
	pmtx = (YabSem_rthreads *)mtx;
#if defined(ARCH_IS_LINUX)
	sem_post(&pmtx->sem);
#elif defined(ARCH_IS_MACOSX)
	dispatch_semaphore_signal(pmtx->sem);
#else
	ReleaseSemaphore(pmtx->sem, 1, NULL);
#endif
}

void YabSemWait( YabSem * mtx )
{
	YabSem_rthreads * pmtx;
	pmtx = (YabSem_rthreads *)mtx;
#if defined(ARCH_IS_LINUX)
	sem_wait(&pmtx->sem);
#elif defined(ARCH_IS_MACOSX)
	dispatch_semaphore_wait(pmtx->sem, DISPATCH_TIME_FOREVER);
#else
	WaitForSingleObject(pmtx->sem, 0L);
#endif
}

YabSem * YabThreadCreateSem(int val)
{
	YabSem_rthreads * mtx = (YabSem_rthreads *)malloc(sizeof(YabSem_rthreads));
#if defined(ARCH_IS_LINUX)
	sem_init( &mtx->sem,0,val);
#elif defined(ARCH_IS_MACOSX)
	dispatch_semaphore_t *sem = &mtx->sem;
	*sem = dispatch_semaphore_create(val);
#else
	mtx->sem = CreateSemaphore( NULL, val, val, NULL);
#endif
	return (YabMutex *)mtx;
}
