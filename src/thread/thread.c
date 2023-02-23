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

struct mthpc_barrier_object {
    struct mthpc_barrier barrier;
    long refcount; /* For cluster-level barrier only */
};

struct mthpc_threads_info {
    struct mthpc_list_head head;
    /* number of thread (group) object */
    unsigned long nr_group;
    spinlock_t lock;
};
static struct mthpc_threads_info mthpc_threads_info;

/*
 * We only take care of the release state of group->barrier_object.
 * Since, it won't be shared during the allocation.
 * Use READ_ONCE()/WRITE_ONCE() here, since mthpc_thread_wq_join()
 * may race with __mthpc_thread_async_wait().
 */
static __always_inline void
mthpc_group_put_barrier_object(struct mthpc_thread_group *group)
{
    if (READ_ONCE(group->barrier_object)) {
        long nr_thread = __atomic_add_fetch(&group->barrier_object->refcount,
                                            -group->nr, __ATOMIC_RELAXED);
        MTHPC_WARN_ON(nr_thread < 0,
                      "the lifetime of barrier object just messed up");
        if (nr_thread == 0)
            free(group->barrier_object);
        WRITE_ONCE(group->barrier_object, NULL);
    }
}

static void mthpc_thread_wq_join(struct mthpc_work *work)
{
    struct mthpc_thread_group *group;
    struct mthpc_list_head *n, *pos;

    spin_lock(&mthpc_threads_info.lock);
    mthpc_list_for_each_safe (pos, n, &mthpc_threads_info.head) {
        group = container_of(pos, struct mthpc_thread_group, node);
        if (READ_ONCE(group->nr_exited) == group->nr) {
            for (int i = 0; i < group->nr; i++)
                pthread_join(group->thread[i], NULL);
            mthpc_list_del(&group->node);
            mthpc_threads_info.nr_group--;
            mthpc_group_put_barrier_object(group);
        }
    }
    if (mthpc_threads_info.nr_group) {
        spin_unlock(&mthpc_threads_info.lock);
        mthpc_queue_thread_work(work);
        return;
    }
    spin_unlock(&mthpc_threads_info.lock);
}

static MTHPC_DECLARE_WORK(mthpc_thread_work, mthpc_thread_wq_join, NULL);

static __always_inline void
mthpc_thread_async_cleanup(struct mthpc_thread_group *group)
{
    __atomic_fetch_add(&group->nr_exited, 1, __ATOMIC_RELEASE);
}

static inline void __mthpc_thread_worker(struct mthpc_thread_group *group)
{
    mthpc_rcu_thread_init();
    if (group->init)
        group->init(group);
    if (group->barrier_object) {
        mthpc_centralized_barrier(&group->barrier_object->barrier,
                                  READ_ONCE(group->barrier_object->refcount));
    }
    if (group->func)
        group->func(group);
    mthpc_rcu_thread_exit();
}

static void *mthpc_thread_worker(void *arg)
{
    struct mthpc_thread_group *group = arg;

    __mthpc_thread_worker(group);
    pthread_exit(NULL);
}

static void *mthpc_thread_async_worker(void *arg)
{
    struct mthpc_thread_group *group = arg;
    __mthpc_thread_worker(group);
    mthpc_thread_async_cleanup(group);
    pthread_exit(NULL);
}

/* User API */

void __mthpc_thread_run(void *threads, unsigned int nr)
{
    struct mthpc_thread_group **ths = (struct mthpc_thread_group **)threads;
    struct mthpc_barrier_object barrier_object = { .barrier =
                                                       MTHPC_BARRIER_INIT };
    unsigned long total_nr_thread = 0;

    for (int i = 0; i < nr; i++) {
        struct mthpc_thread_group *group = ths[i];

        MTHPC_WARN_ON(group->type & MTHPC_THREAD_ASYNC_TYPE,
                      "async thread rerun on sync");

        total_nr_thread += group->nr;
    }

    WRITE_ONCE(barrier_object.refcount, total_nr_thread);

    for (int i = 0; i < nr; i++) {
        struct mthpc_thread_group *group = ths[i];

        group->barrier_object = &barrier_object;
        for (int j = 0; j < group->nr; j++)
            pthread_create(&group->thread[j], NULL, mthpc_thread_worker, group);
    }

    for (int i = 0; i < nr; i++) {
        struct mthpc_thread_group *group = ths[i];

        for (int j = 0; j < group->nr; j++)
            pthread_join(group->thread[j], NULL);
    }
}

void __mthpc_thread_async_run(void *threads, unsigned int nr)
{
    struct mthpc_thread_group **ths = (struct mthpc_thread_group **)threads;
    struct mthpc_barrier_object *barrier_object =
        malloc(sizeof(struct mthpc_barrier_object));
    unsigned long total_nr_thread = 0;

    if (!barrier_object) {
        MTHPC_WARN_ON(1,
                      "Cannot allocate barrier, failed to create async thread");
        return;
    }

    mthpc_barrier_init(&barrier_object->barrier);

    for (int i = 0; i < nr; i++) {
        struct mthpc_thread_group *group = ths[i];
        total_nr_thread += group->nr;
    }

    WRITE_ONCE(barrier_object->refcount, total_nr_thread);

    for (int i = 0; i < nr; i++) {
        struct mthpc_thread_group *group = ths[i];

        group->type |= MTHPC_THREAD_ASYNC_TYPE;

        mthpc_list_init(&group->node);
        spin_lock(&mthpc_threads_info.lock);
        mthpc_list_add_tail(&group->node, &mthpc_threads_info.head);
        mthpc_threads_info.nr_group++;
        spin_unlock(&mthpc_threads_info.lock);

        group->barrier_object = barrier_object;
        for (int j = 0; j < group->nr; j++)
            pthread_create(&group->thread[j], NULL, mthpc_thread_async_worker,
                           group);
    }

    mthpc_queue_thread_work(&mthpc_thread_work);
}

void __mthpc_thread_async_wait(void *threads, unsigned int nr)
{
    struct mthpc_thread_group **ths = (struct mthpc_thread_group **)threads;

    smp_mb();

    for (int i = 0; i < nr; i++) {
        struct mthpc_thread_group *group = ths[i];

        MTHPC_BUG_ON(!(group->type & MTHPC_THREAD_ASYNC_TYPE),
                     "non-async thread oject wait");

        /*
         * Remove the node since async wait may finish in workqueue_exit().
         * At the time, the thread (might be the stack memory) could cause
         * the segfault.
         */
        spin_lock(&mthpc_threads_info.lock);
        mthpc_list_del(&group->node);
        mthpc_threads_info.nr_group--;
        spin_unlock(&mthpc_threads_info.lock);

        for (int j = 0; j < group->nr; j++) {
            while (__atomic_load_n(&group->nr_exited, __ATOMIC_ACQUIRE) !=
                   group->nr)
                mthpc_cmb();
        }
        for (int i = 0; i < group->nr; i++)
            pthread_join(group->thread[i], NULL);
        mthpc_group_put_barrier_object(group);
    }

    smp_mb();
}

/* init/exit function */

static __mthpc_init void mthpc_thread_init(void)
{
    mthpc_init_feature();
    mthpc_threads_info.nr_group = 0;
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
