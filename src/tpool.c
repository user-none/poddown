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

#include <stdlib.h>
#include <pthread.h>

#include "tpool.h"
#include "cpthread.h"
#include "xmem.h"

/* - - - - */

/*! Work object which will sit in a queue
 * waiting for the pool to process it.
 *
 * It is a singly linked list acting as a FIFO queue. */
struct tpool_work {
    thread_func_t      func;  /*!< Function to be called. */
    void              *arg;   /*!< Data to be passed to func. */
    struct tpool_work *next;  /*!< Next work item in the queue. */
};
typedef struct tpool_work tpool_work_t;

struct tpool {
    tpool_work_t    *work_first;   /*!< First work item in the work queue. */
    tpool_work_t    *work_last;    /*!< Last work item in the work queue. */
    pthread_mutex_t  work_mutex;   /*<! Mutex protecting inserting and removing work from the work queue. */
    pthread_cond_t   work_cond;    /*!< Conditional to signal when there is work to process. */
    pthread_cond_t   working_cond; /*!< Conditional to signal when there is no work processing.
                                        This will also signal when there are no threads running. */
    size_t           working_cnt;  /*!< The number of threads processing work (Not waiting for work). */
    size_t           thread_cnt;   /*!< Total number of threads within the pool. */
    bool             stop;         /*!< Marker to tell the work threads to exit. */
};

/* - - - - */

static tpool_work_t *tpool_work_create(thread_func_t func, void *arg)
{
    tpool_work_t *work;

    if (func == NULL)
        return NULL;

    work       = xcalloc(1, sizeof(*work));
    work->func = func;
    work->arg  = arg;
    work->next = NULL;
    return work;
}

static void tpool_work_destroy(tpool_work_t *work)
{
    if (work == NULL)
        return;
    xfree(work);
}

/*!< Pull the first work item out of the queue. */
static tpool_work_t *tpool_work_get(tpool_t *tp)
{
    tpool_work_t *work;

    if (tp == NULL)
        return NULL;

    work = tp->work_first;
    if (work == NULL)
        return NULL;

    if (work->next == NULL) {
        tp->work_first = NULL;
        tp->work_last  = NULL;
    } else {
        tp->work_first = work->next;
    }

    return work;
}

static void *tpool_worker(void *arg)
{
    tpool_t      *tp = arg;
    tpool_work_t *work;

    while (1) {
        pthread_mutex_lock(&(tp->work_mutex));
        /* Keep running until told to stop. */
        if (tp->stop)
            break;

        /* If there is no work in the queue wait in the conditional until
         * there is work to take. */
        if (tp->work_first == NULL)
            pthread_cond_wait(&(tp->work_cond), &(tp->work_mutex));

        /* Try to pull work from the queue. */
        work = tpool_work_get(tp);
        tp->working_cnt++;
        pthread_mutex_unlock(&(tp->work_mutex));

        /* Call the work function and let it process.
         *
         * work can legitimately be NULL. Since multiple threads from the pool
         * will wake when there is work, a thread might not get any work. 1
         * piece of work and 2 threads, both will wake but with 1 only work 1
         * will get the work and the other won't.
         *
         * working_cnt has been increment and work could be NULL. While it's
         * not true there is work processing the thread is considered working
         * because it's not waiting in the conditional. Pedantic but...
         */
        if (work != NULL) {
            work->func(work->arg);
            tpool_work_destroy(work);
        }

        pthread_mutex_lock(&(tp->work_mutex));
        tp->working_cnt--;
        /* Since we're in a lock no work can be added or removed form the queue.
         * Also, the working_cnt can't be changed (except the thread holding the lock).
         * At this point if there isn't any work processing and if there is no work
         * signal this is the case. */
        if (!tp->stop && tp->working_cnt == 0 && tp->work_first == NULL)
            pthread_cond_signal(&(tp->working_cond));
        pthread_mutex_unlock(&(tp->work_mutex));
    }

    tp->thread_cnt--;
    if (tp->thread_cnt == 0)
        pthread_cond_signal(&(tp->working_cond));
    pthread_mutex_unlock(&(tp->work_mutex));
    return NULL;
}

/* - - - - */

tpool_t *tpool_create(size_t num)
{
    tpool_t   *tp;
    pthread_t  thread;
    size_t     i;

    if (num == 0)
        num = 2;

    tp             = xcalloc(1, sizeof(*tp));
    tp->thread_cnt = num;

    pthread_mutex_init(&(tp->work_mutex), NULL);
    pthread_cond_init(&(tp->work_cond), NULL);
    pthread_cond_init(&(tp->working_cond), NULL);

    tp->work_first = NULL;
    tp->work_last  = NULL;

    /* Create the requested number of thread and detach them. */
    for (i=0; i<num; i++) {
        pthread_create(&thread, NULL, tpool_worker, tp);
        pthread_detach(thread);
    }

    return tp;
}

void tpool_destroy(tpool_t *tp)
{
    tpool_work_t *work;
    tpool_work_t *work2;

    if (tp == NULL)
        return;

    /* Take all work out of the queue and destroy it. */
    pthread_mutex_lock(&(tp->work_mutex));
    work = tp->work_first;
    while (work != NULL) {
        work2 = work->next;
        tpool_work_destroy(work);
        work = work2;
    }
    tp->work_first = NULL;
    /* Tell the worker threads to stop. */
    tp->stop = true;
    pthread_cond_broadcast(&(tp->work_cond));
    pthread_mutex_unlock(&(tp->work_mutex));

    /* Wait for all threads to stop. */
    tpool_wait(tp);

    pthread_mutex_destroy(&(tp->work_mutex));
    pthread_cond_destroy(&(tp->work_cond));
    pthread_cond_destroy(&(tp->working_cond));

    xfree(tp);
}

/* - - - - */

bool tpool_add_work(tpool_t *tp, thread_func_t func, void *arg)
{
    tpool_work_t *work;

    if (tp == NULL)
        return false;

    work = tpool_work_create(func, arg);
    if (work == NULL)
        return false;

    pthread_mutex_lock(&(tp->work_mutex));
    if (tp->work_first == NULL) {
        tp->work_first = work;
        tp->work_last  = tp->work_first;
    } else {
        tp->work_last->next = work;
        tp->work_last       = work;
    }

    pthread_cond_broadcast(&(tp->work_cond));
    pthread_mutex_unlock(&(tp->work_mutex));

    return true;
}

void tpool_wait(tpool_t *tp)
{
    if (tp == NULL)
        return;

    pthread_mutex_lock(&(tp->work_mutex));
    while (1) {
        /* working_cond is dual use. It signals when we're not stopping but the
         * working_cnt is 0 indicating there isn't any work processing. If we
         * are stopping it will trigger when there aren't any threads running. */
        if (tp->work_first != NULL || (!tp->stop && tp->working_cnt != 0) || (tp->stop && tp->thread_cnt != 0)) {
            pthread_cond_wait(&(tp->working_cond), &(tp->work_mutex));
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&(tp->work_mutex));
}
