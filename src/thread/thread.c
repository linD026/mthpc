#include <stdlib.h>
#include <pthread.h>

#include <mthpc/thread.h>
#include <mthpc/centralized_barrier.h>
#include <mthpc/rcu.h>
#include <mthpc/workqueue.h>
#include <mthpc/list.h>

#include <internal/workqueue.h>

#include <internal/feature.h>
#undef _MTHPC_FEATURE
#define _MTHPC_FEATURE thread

struct mthpc_threads_info {
    struct mthpc_list_head head;
    /* number of thread (group) object */
    unsigned long nr;
    pthread_mutex_t lock;
};
static struct mthpc_threads_info mthpc_threads_info;

static void mthpc_thread_wq_join(struct mthpc_work *work)
{
    struct mthpc_thread *thread;
    struct mthpc_list_head *n, *pos;

    pthread_mutex_lock(&mthpc_threads_info.lock);
    mthpc_list_for_each_safe (pos, n, &mthpc_threads_info.head) {
        thread = container_of(pos, struct mthpc_thread, node);
        if (READ_ONCE(thread->nr_exited) == thread->nr) {
            for (int i = 0; i < thread->nr; i++)
                pthread_join(thread->thread[i], NULL);
            mthpc_list_del(&thread->node);
            mthpc_threads_info.nr--;
        }
    }
    if (mthpc_threads_info.nr)
        mthpc_thread_work_on(work);
    pthread_mutex_unlock(&mthpc_threads_info.lock);
}

static MTHPC_DECLARE_WORK(mthpc_thread_work, mthpc_thread_wq_join, NULL);

static __always_inline void
mthpc_thread_async_cleanup(struct mthpc_thread *thread)
{
    __atomic_fetch_add(&thread->nr_exited, 1, __ATOMIC_RELEASE);
}

static inline void __mthpc_thread_worker(struct mthpc_thread *thread)
{
    mthpc_rcu_thread_init();
    if (thread->init)
        thread->init(thread);
    if (thread->barrier)
        mthpc_centralized_barrier(thread->barrier, thread->nr);
    if (thread->func)
        thread->func(thread);
    mthpc_rcu_thread_exit();
}

static void *mthpc_thread_worker(void *arg)
{
    struct mthpc_thread *thread = arg;

    __mthpc_thread_worker(thread);
    pthread_exit(NULL);
}

static void *mthpc_thread_async_worker(void *arg)
{
    struct mthpc_thread *thread = arg;
    __mthpc_thread_worker(thread);
    mthpc_thread_async_cleanup(thread);
    pthread_exit(NULL);
}

void __mthpc_thread_run(void *threads, unsigned int nr)
{
    struct mthpc_thread **ths = (struct mthpc_thread **)threads;
    struct mthpc_barrier barrier = MTHPC_BARRIER_INIT;

    for (int i = 0; i < nr; i++) {
        struct mthpc_thread *thread = ths[i];

        MTHPC_WARN_ON(thread->type & MTHPC_THREAD_ASYNC_TYPE,
                      "async thread rerun on sync");

        thread->barrier = &barrier;
        for (int j = 0; j < thread->nr; j++)
            pthread_create(&thread->thread[j], NULL, mthpc_thread_worker,
                           thread);
    }

    for (int i = 0; i < nr; i++) {
        struct mthpc_thread *thread = ths[i];

        for (int j = 0; j < thread->nr; j++)
            pthread_join(thread->thread[j], NULL);
    }
}

void __mthpc_thread_async_run(void *threads, unsigned int nr)
{
    struct mthpc_thread **ths = (struct mthpc_thread **)threads;
    struct mthpc_barrier barrier = MTHPC_BARRIER_INIT;

    for (int i = 0; i < nr; i++) {
        struct mthpc_thread *thread = ths[i];

        thread->type |= MTHPC_THREAD_ASYNC_TYPE;

        mthpc_list_init(&thread->node);
        pthread_mutex_lock(&mthpc_threads_info.lock);
        mthpc_list_add_tail(&thread->node, &mthpc_threads_info.head);
        mthpc_threads_info.nr++;
        pthread_mutex_unlock(&mthpc_threads_info.lock);

        thread->barrier = &barrier;
        for (int j = 0; j < thread->nr; j++)
            pthread_create(&thread->thread[j], NULL, mthpc_thread_async_worker,
                           thread);
    }

    mthpc_thread_work_on(&mthpc_thread_work);
}

static __mthpc_init void mthpc_thread_init(void)
{
    mthpc_init_feature();
    mthpc_threads_info.nr = 0;
    pthread_mutex_init(&mthpc_threads_info.lock, NULL);
    mthpc_list_init(&mthpc_threads_info.head);
    mthpc_init_ok();
}

static __mthpc_exit void mthpc_thread_exit(void)
{
    mthpc_exit_feature();
    pthread_mutex_destroy(&mthpc_threads_info.lock);
    mthpc_exit_ok();
}
