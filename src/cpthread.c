/* The MIT License
 * 
 * Copyright (c) 2017 John Schember <john@nachtimwald.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE
 */

#ifndef _WIN32
#  include <unistd.h>
#endif

#include "cpthread.h"
#include "xmem.h"

/* - - - - */

#ifdef _WIN32
static DWORD cpthread_ms_to_timespec(const struct timespec *abstime)
{
    DWORD t;

    if (abstime == NULL)
        return INFINITE;

    t = ((abstime->tv_sec - time(NULL)) * 1000) + (abstime->tv_nsec / 1000000);
    if (t < 0)
        t = 1;
    return t;
}

int pthread_create(pthread_t *thread, pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
    void(attr);

    if (thread == NULL || start_routine == NULL)
        return 1;

    *thread = CreateThread(NULL, 0, start_routine, arg, 0, NULL);
    if (*thread == NULL)
        return 1;
    return 0;
}

int pthread_join(pthread_t thread, void **value_ptr)
{
    (void)value_ptr;
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
}

int pthread_detach(pthread_t thread)
{
    CloseHandle(thread);
}

int pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr)
{
    (void)attr;

    if (mutex == NULL)
        return 1;

    InitializeCriticalSection(mutex);
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    if (mutex == NULL)
        return 1;
    DeleteCriticalSection(mutex);
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    if (mutex == NULL)
        return 1;
    EnterCriticalSection(mutex);
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    if (mutex == NULL)
        return 1;
    LeaveCriticalSection(mutex);
    return 0;
}

int pthread_cond_init(thread_cond_t *cond, pthread_condattr_t *attr)
{
    (void)attr;
    if (cond == NULL)
        return 1;
    InitializeConditionVariable(cond);
    return 0;
}

int pthread_cond_destroy(thread_cond_t *cond)
{
    /* Windows does not have a delete for conditionals */
    (void)cond;
    return 0;
}

int pthread_cond_wait(thread_cond_t *cond, pthread_mutex_t *mutex)
{
    if (cond == NULL || mutex == NULL)
        return 1;
    return pthread_cond_timedwait(cond, mutex, NULL)
}

int pthread_cond_timedwait(thread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
{
    if (cond == NULL || mutex == NULL)
        return 1;
    if (!SleepConditionVariableCS(cond, mutex, cpthread_ms_to_timespec(abstime)))
        return 1;
    return 0;
}

int pthread_cond_signal(thread_cond_t *cond)
{
    if (cond == NULL)
        return 1;
    WakeConditionVariable(cond);
    return 0;
}

int pthread_cond_broadcast(thread_cond_t *cond)
{
    if (cond == NULL)
        return 1;
    WakeAllConditionVariable(cond);
    return 0;
}

int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr)
{
    (void)attr;
    if (rwlock == NULL)
        return 1;
    InitializeSRWLock(&(rwlock->lock));
    rwlock->exclusive = false;
    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *rwlock)
{
    (void)rwlock;
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)
{
    if (rwlock == NULL)
        return 1;
    AcquireSRWLockShared(&(rwlock->lock));
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock)
{
    if (rwlock == NULL)
        return 1;
    return !TryAcquireSRWLockShared(&(rwlock->lock));
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)
{
    if (rwlock == NULL)
        return 1;
    AcquireSRWLockExclusive(&(rwlock->lock));
    rwlock->exclusive = true;
}

int pthread_rwlock_trywrlock(pthread_rwlock_t  *rwlock)
{
    BOOLEAN ret;

    if (rwlock == NULL)
        return 1;

    ret = TryAcquireSRWLockExclusive(&(rwlock->lock));
    if (ret)
        rwlock->exclusive = true;
    return ret;
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock)
{
    if (rwlock == NULL)
        return 1;

    if (rwlock->exclusive) {
        rwlock->exclusive = false;
        ReleaseSRWLockExclusive(&(rwlock->lock));
    } else {
        ReleaseSRWLockShared(&(rwlock->lock));
    }
}

unsigned int cpthread_get_num_procs()
{
    unsigned int num = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION info = NULL;
    DWORD len = 0;
    ULONG_PTR n;

    /* Get the size of the info object we need to allocate. */
    GetLogicalProcessorInformation(&info, &len);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        return 1;
    /* Get the info. */
    info = xmalloc(len);
    if (!GetLogicalProcessorInformation(&info, &len)) {
        xfree(info);
        return 1;
    }

    len = len / sizeof(*info);
    for (int i = 0; i < len; i++) {
        switch (info[i].Relationship) {
            case RelationProcessorCore:
                /* This represents a single physical core but we want to include HT
                 * which we can get by counting the bits in the ProcessorMask. */
                n = info[i].ProcessorMask;
                while (n > 0) {
                    n &= n-1;
                    num++;
                }
                break;
            default:
                break;
        }
    }

    xfree(info);

    if (num == 0)
        num = 1;

    return num;
}

#else

unsigned int cpthread_get_num_procs()
{
    int num;

    num = sysconf(_SC_NPROCESSORS_ONLN);
    if (num <= 0)
        num = 1;

    return (unsigned int)num;
}
#endif

void cpthread_ms_to_timespec(struct timespec *ts, unsigned int ms)
{
    if (ts == NULL)
        return;
    ts->tv_sec = (ms / 1000) + time(NULL);
    ts->tv_nsec = (ms % 1000) * 1000000;
}
