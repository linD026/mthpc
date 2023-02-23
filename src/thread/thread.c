#include <stdlib.h>

#include <mthpc/thread.h>
#include <mthpc/spinlock.h>
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
    spinlock_t lock;
};
static struct mthpc_threads_info mthpc_threads_info;

static void mthpc_thread_wq_join(struct mthpc_work *work)
{
    struct mthpc_thread_group *thread;
    struct mthpc_list_head *n, *pos;

    spin_lock(&mthpc_threads_info.lock);
    mthpc_list_for_each_safe (pos, n, &mthpc_threads_info.head) {
        thread = container_of(pos, struct mthpc_thread, node);
        if (READ_ONCE(thread->nr_exited) == thread->nr) {
            for (int i = 0; i < thread->nr; i++)
                pthread_join(thread->thread[i], NULL);
            mthpc_list_del(&thread->node);
            mthpc_threads_info.nr--;
        }
    }
    if (mthpc_threads_info.nr) {
        spin_unlock(&mthpc_threads_info.lock);
        mthpc_queue_thread_work(work);
        return;
    }
    spin_unlock(&mthpc_threads_info.lock);
}

static MTHPC_DECLARE_WORK(mthpc_thread_work, mthpc_thread_wq_join, NULL);

static __always_inline void
mthpc_thread_async_cleanup(struct mthpc_thread_group *thread)
{
    __atomic_fetch_add(&thread->nr_exited, 1, __ATOMIC_RELEASE);
}

static inline void __mthpc_thread_worker(struct mthpc_thread_group *thread)
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
    struct mthpc_thread_group *thread = arg;

    __mthpc_thread_worker(thread);
    pthread_exit(NULL);
}

static void *mthpc_thread_async_worker(void *arg)
{
    struct mthpc_thread_group *thread = arg;
    __mthpc_thread_worker(thread);
    mthpc_thread_async_cleanup(thread);
    pthread_exit(NULL);
}

/* User API */

void __mthpc_thread_run(void *threads, unsigned int nr)
{
    struct mthpc_thread_group **ths = (struct mthpc_thread_group **)threads;
    struct mthpc_barrier barrier = MTHPC_BARRIER_INIT;

    for (int i = 0; i < nr; i++) {
        struct mthpc_thread_group *thread = ths[i];

        MTHPC_WARN_ON(thread->type & MTHPC_THREAD_ASYNC_TYPE,
                      "async thread rerun on sync");

        thread->barrier = &barrier;
        for (int j = 0; j < thread->nr; j++)
            pthread_create(&thread->thread[j], NULL, mthpc_thread_worker,
                           thread);
    }

    for (int i = 0; i < nr; i++) {
        struct mthpc_thread_group *thread = ths[i];

        for (int j = 0; j < thread->nr; j++)
            pthread_join(thread->thread[j], NULL);
    }
}

void __mthpc_thread_async_run(void *threads, unsigned int nr)
{
    struct mthpc_thread_group **ths = (struct mthpc_thread_group **)threads;
    // TODO: How can we handle the barrier resource within heap?
    //struct mthpc_barrier barrier = MTHPC_BARRIER_INIT;

    for (int i = 0; i < nr; i++) {
        struct mthpc_thread_group *thread = ths[i];

        thread->type |= MTHPC_THREAD_ASYNC_TYPE;

        mthpc_list_init(&thread->node);
        spin_lock(&mthpc_threads_info.lock);
        mthpc_list_add_tail(&thread->node, &mthpc_threads_info.head);
        mthpc_threads_info.nr++;
        spin_unlock(&mthpc_threads_info.lock);

        //thread->barrier = &barrier;
        for (int j = 0; j < thread->nr; j++)
            pthread_create(&thread->thread[j], NULL, mthpc_thread_async_worker,
                           thread);
    }

    mthpc_queue_thread_work(&mthpc_thread_work);
}

void __mthpc_thread_async_wait(void *threads, unsigned int nr)
{
    struct mthpc_thread_group **ths = (struct mthpc_thread_group **)threads;

    smp_mb();

    for (int i = 0; i < nr; i++) {
        struct mthpc_thread_group *thread = ths[i];

        MTHPC_BUG_ON(!(thread->type & MTHPC_THREAD_ASYNC_TYPE),
                     "non-async thread oject wait");

        /*
         * Remove the node since async wait may finish in workqueue_exit().
         * At the time, the thread (might be the stack memory) could cause
         * the segfault.
         */
        spin_lock(&mthpc_threads_info.lock);
        mthpc_list_del(&thread->node);
        mthpc_threads_info.nr--;
        spin_unlock(&mthpc_threads_info.lock);

        for (int j = 0; j < thread->nr; j++)
            while (__atomic_load_n(&thread->nr_exited, __ATOMIC_ACQUIRE) !=
                   thread->nr)
                mthpc_cmb();
        for (int i = 0; i < thread->nr; i++)
            pthread_join(thread->thread[i], NULL);
    }

    smp_mb();
}

/* init/exit function */

static __mthpc_init void mthpc_thread_init(void)
{
    mthpc_init_feature();
    mthpc_threads_info.nr = 0;
    spin_lock_init(&mthpc_threads_info.lock);
    mthpc_list_init(&mthpc_threads_info.head);
    mthpc_init_ok();
}

static __mthpc_exit void mthpc_thread_exit(void)
{
    mthpc_exit_feature();
    // TODO: Fix the spinlock destroyed while other thread still holding lock
    spin_lock(&mthpc_threads_info.lock);
    spin_unlock(&mthpc_threads_info.lock);
    spin_lock_destroy(&mthpc_threads_info.lock);
    mthpc_exit_ok();
}
