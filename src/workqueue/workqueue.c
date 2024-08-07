#define _GNU_SOURCE
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <stdatomic.h>

#include <mthpc/workqueue.h>
#include <mthpc/spinlock.h>
#include <mthpc/util.h>
#include <mthpc/debug.h>
#include <mthpc/print.h> /* for dump work */
#include <mthpc/rcu.h> /* queue work will use rcu */
#include <mthpc/rculist.h>
#include <mthpc/futex.h>

#include <internal/workqueue.h> /* provide wq for other features */
#include <internal/rcu.h> /* for rcu workqueue */

#include <internal/feature.h>
#undef _MTHPC_FEATURE
#define _MTHPC_FEATURE workqueue

#define MTHPC_WQ_NR_CPU (4U)
#define MTHPC_WQ_CPU_MASK (MTHPC_WQ_NR_CPU - 1)
#define MTHPC_WQ_ACTIVED_FLAG (MTHPC_WQ_NR_CPU)

struct mthpc_workqueue {
    /* pool uses active to notify the wq should be finished or not. */
    atomic_uint __actived_cpu;
    spinlock_t lock;
    pthread_t tid;
    /*
     * Following futex values represent the of the workqueue:
     * - 0: empty queue
     * - 1: non-empty queue
     */
    int32_t futex;
    /* the number of work in queue */
    unsigned int count;
    /* workqueue (work) linked list */
    struct mthpc_list_head head;
    /* workpool linked list */
    struct mthpc_list_head node;
    struct mthpc_workpool *wp;
};

struct mthpc_workpool {
    const char *name;
    struct mthpc_list_head head;
    /*
     * The fast path will access count without holding the lock.
     * So, use atomic ops to access count even it's protected
     * by lock.
     */
    atomic_uint count;

    spinlock_t lock;
} __mthpc_aligned__;
static struct mthpc_workpool mthpc_workpool;
static struct mthpc_workpool mthpc_thread_wp;
static struct mthpc_workpool mthpc_taskflow_wp;
//static struct mthpc_workpool mthpc_rcu_wp;

static __always_inline int mthpc_wq_get_cpu(struct mthpc_workqueue *wq)
{
#ifdef __linux__
    return (
        int)(atomic_load_explicit(&wq->__actived_cpu, memory_order_relaxed) &
             MTHPC_WQ_CPU_MASK);
#else
    return 0;
#endif
}

static __always_inline void mthpc_wq_set_cpu(struct mthpc_workqueue *wq,
                                             int cpu)
{
#ifdef __linux__
    unsigned int old_masked_cpuid, masked_cpuid;

    masked_cpuid =
        atomic_load_explicit(&wq->__actived_cpu, memory_order_acquire);

    old_masked_cpuid = masked_cpuid;
    masked_cpuid &= ~MTHPC_WQ_CPU_MASK;
    masked_cpuid |= (cpu & MTHPC_WQ_CPU_MASK);

    atomic_compare_exchange_strong_explicit(
        &wq->__actived_cpu, &old_masked_cpuid, masked_cpuid,
        memory_order_release, memory_order_relaxed);
#else
#endif
}

static __always_inline void mthpc_wq_run_on_cpu(struct mthpc_workqueue *wq)
{
#ifdef __linux__
    unsigned int cpuid = mthpc_wq_get_cpu(wq);
    pthread_t tid = pthread_self();
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(cpuid, &cpuset);

    MTHPC_WARN_ON(pthread_setaffinity_np(tid, sizeof(cpuset), &cpuset),
                  "set cpu failed");
#else
#endif
}

static __always_inline bool mthpc_wq_active(struct mthpc_workqueue *wq)
{
    unsigned int masked_active = 0;

    masked_active =
        atomic_load_explicit(&wq->__actived_cpu, memory_order_acquire);

    return masked_active & MTHPC_WQ_ACTIVED_FLAG;
}

static __always_inline void mthpc_wq_mkactive(struct mthpc_workqueue *wq)
{
    atomic_fetch_or_explicit(&wq->__actived_cpu, MTHPC_WQ_ACTIVED_FLAG,
                             memory_order_release);
}

static __always_inline void mthpc_wq_clear_active(struct mthpc_workqueue *wq)
{
    atomic_fetch_and_explicit(&wq->__actived_cpu, ~MTHPC_WQ_ACTIVED_FLAG,
                              memory_order_release);
}

static __always_inline void mthpc_wq_futex_wait(struct mthpc_workqueue *wq)
{
    int ret = 0;

    smp_rmb();
    while (!READ_ONCE(wq->futex)) {
        ret = futex((int32_t *)&wq->futex, FUTEX_WAIT, 0, NULL, NULL, 0);
        switch (ret) {
        case EAGAIN:
            /* value already changed, someone queue the work */
            /* we assume that the wq thread will stay in same cpu. */
            WRITE_ONCE(wq->futex, 0);
            return;
        case EINTR:
            smp_rmb();
            break;
        default:
            MTHPC_BUG_ON(1, "futex(&wq->futex, FUTEX_WAIT, 0)");
        }
    }
}

static __always_inline void mthpc_wq_futex_wake(struct mthpc_workqueue *wq)
{
    WRITE_ONCE(wq->futex, 1);
    atomic_thread_fence(memory_order_release);
    futex(&wq->futex, FUTEX_WAKE, 1, NULL, NULL, 0);
}

static struct mthpc_workqueue *mthpc_alloc_workqueue(void)
{
    struct mthpc_workqueue *wq = malloc(sizeof(struct mthpc_workqueue));
    if (!wq)
        return NULL;

    wq->count = 0;
    wq->futex = 0;
    spin_lock_init(&wq->lock);
    mthpc_list_init(&wq->head);
    mthpc_list_init(&wq->node);
    mthpc_wq_mkactive(wq);
    /* we set the wq to its cpu when running the thread. */

    return wq;
}

static void *mthpc_worker_run(void *arg)
{
    struct mthpc_workqueue *wq = arg;
    struct mthpc_work *work;

    // Sometime, when we do the rcu init in rcu_read_lock() will let
    // mthpc_rcu_node_ptr become NULL but aleady add to rcu list?
    mthpc_rcu_thread_init();
    mthpc_wq_run_on_cpu(wq);

    while (1) {
        if (!mthpc_wq_active(wq))
            break;
        spin_lock(&wq->lock);
        if (mthpc_list_empty(&wq->head)) {
            MTHPC_WARN_ON(wq->count > 0, "list is empty but count=%u\n",
                          wq->count);
            spin_unlock(&wq->lock);
            mthpc_wq_futex_wait(wq);
        } else {
            work = container_of(wq->head.next, struct mthpc_work, node);
            mthpc_list_del(&work->node);
            wq->count--;
            spin_unlock(&wq->lock);

            /* We shouldn't hold the lock when running work. */
            // TODO: provide the container option?
            work->func(work);
        }
    }

    mthpc_rcu_thread_exit();

    pthread_exit(NULL);
}

static __always_inline void mthpc_works_handler(struct mthpc_workqueue *wq)
{
    pthread_create(&wq->tid, NULL, mthpc_worker_run, wq);
}

static void inline mthpc_workqueue_add_locked(struct mthpc_workqueue *wq,
                                              struct mthpc_work *work)
{
    /* Prevent mthpc_workqueues_join() to delete the wq. */
    mthpc_list_add_tail_rcu(&work->node, &wq->head);
    wq->count++;
    MTHPC_WARN_ON(!mthpc_wq_active(wq), "Add work to inactive wq");
    work->wq = wq;
}

/* get global workqueue */
static struct mthpc_workqueue *
mthpc_get_workqueue(struct mthpc_workpool *wp, int cpu, struct mthpc_work *work)
{
    struct mthpc_workqueue *wq = NULL, *prealloc = NULL;
    struct mthpc_workqueue *curr;
    struct mthpc_list_head *pos;

    /* fast path - find the existed first. */
    mthpc_rcu_read_lock();
    mthpc_list_for_each_entry_rcu (curr, &wp->head, node) {
        if (cpu == -1 || (cpu & MTHPC_WQ_CPU_MASK) == mthpc_wq_get_cpu(curr)) {
            wq = curr;
            spin_lock(&wq->lock);
            mthpc_workqueue_add_locked(wq, work);
            spin_unlock(&wq->lock);
            break;
        }
    }
    mthpc_rcu_read_unlock();
    if (wq)
        goto out;

    /* Slow path, step 1 - allocate workqueue */
    prealloc = mthpc_alloc_workqueue();
    if (!prealloc)
        return NULL;
    if (cpu == -1)
#ifdef __linux__
        cpu = (sched_getcpu() + 1) & MTHPC_WQ_CPU_MASK;
#else
        cpu = atomic_load_explicit(&wp->count, memory_order_consume) &
              MTHPC_WQ_CPU_MASK;
#endif
    mthpc_wq_set_cpu(prealloc, cpu);
    prealloc->wp = wp;

    /* We can safely add the work before we add wq to the pool. */
    mthpc_workqueue_add_locked(prealloc, work);

    /* Slow path, step 2 - add to the pool */
    spin_lock(&wp->lock);
    /* Does anyone already create it? Check again */
    mthpc_list_for_each (pos, &wp->head) {
        struct mthpc_workqueue *tmp =
            container_of(pos, struct mthpc_workqueue, node);
        if (cpu == mthpc_wq_get_cpu(tmp)) {
            wq = tmp;
            spin_lock(&wq->lock);
            mthpc_workqueue_add_locked(wq, work);
            spin_unlock(&wq->lock);
            goto unlock;
        }
    }
    /* No one created it, we can safely add to the pool.*/
    mthpc_list_add_tail_rcu(&prealloc->node, &wp->head);
    atomic_fetch_add_explicit(&wp->count, 1, memory_order_relaxed);
    wq = prealloc;
    prealloc = NULL;
unlock:
    spin_unlock(&wp->lock);
    /*
     * If we clear the prealloc, this means that we are going to create new wq.
     */
    if (!prealloc)
        mthpc_works_handler(wq);

out:
    if (prealloc)
        free(prealloc);

    return wq;
}

static int __mthpc_schedule_work_on(struct mthpc_workpool *wp, int cpu,
                                    struct mthpc_work *work)
{
    struct mthpc_workqueue *wq;

    mthpc_list_init(&work->node);
    wq = mthpc_get_workqueue(wp, cpu, work);
    if (!wq)
        return -ENOMEM;

    mthpc_wq_futex_wake(wq);

    return 0;
}

/* internal API */

int mthpc_queue_thread_work(struct mthpc_work *work)
{
    return __mthpc_schedule_work_on(&mthpc_thread_wp, -1, work);
}

int mthpc_schedule_taskflow_work_on(int cpu, struct mthpc_work *work)
{
    return __mthpc_schedule_work_on(&mthpc_taskflow_wp, cpu, work);
}

/* user API */

int mthpc_schedule_work_on(int cpu, struct mthpc_work *work)
{
    return __mthpc_schedule_work_on(&mthpc_workpool, cpu, work);
}

int mthpc_queue_work(struct mthpc_work *work)
{
    return mthpc_schedule_work_on(-1, work);
}

void mthpc_dump_work(struct mthpc_work *work)
{
    struct mthpc_workqueue *wq = work->wq;
    struct mthpc_workpool *wp = wq->wp;

    mthpc_print("Workqueue dump: pool: %s, queue: %p, work: %s\n", wp->name, wq,
                work->name);
    mthpc_print("CPU: %u TID: %lx func: %p private: %p\n", mthpc_wq_get_cpu(wq),
                (unsigned long)wq->tid, work->func, work->private);
    mthpc_dump_stack();
}

/* init/exit function */

static void mthpc_workqueues_join(struct mthpc_workpool *wp)
{
    struct mthpc_list_head *curr, *n;
    struct mthpc_list_head free_list;
    unsigned int progress = 0;

    while (1) {
        if (!atomic_load_explicit(&wp->count, memory_order_relaxed))
            break;
        if (progress >= 32) {
            progress = 0;
            sched_yield();
            continue;
        }

        mthpc_list_init(&free_list);

        spin_lock(&wp->lock);
        mthpc_list_for_each_safe (curr, n, &wp->head) {
            struct mthpc_workqueue *wq =
                container_of(curr, struct mthpc_workqueue, node);

            spin_lock(&wq->lock);
            /* We should waiting for the works in wq. */
            if (!wq->count) {
                mthpc_list_del_rcu(&wq->node);
                // TODO: Could we avoid synchronize with hoding two locks?
                mthpc_synchronize_rcu();
                mthpc_list_add(&wq->node, &free_list);
                mthpc_wq_clear_active(wq);
            }
            spin_unlock(&wq->lock);
        }
        spin_unlock(&wp->lock);

        if (!mthpc_list_empty(&free_list)) {
            mthpc_list_for_each_safe (curr, n, &free_list) {
                struct mthpc_workqueue *wq =
                    container_of(curr, struct mthpc_workqueue, node);

                pthread_join(wq->tid, NULL);
                /*
                 * We don't have to hold lock anymore, however we
                 * should still make sure the read access is atomic ops.
                 * It's fine if the value is unstable.
                 */
                MTHPC_WARN_ON(
                    atomic_load_explicit(
                        (volatile _Atomic __typeof__(wq->count) *)&wq->count,
                        memory_order_relaxed) > 0,
                    "freeing wq but still holding work(s)");
                spin_lock_destroy(&wq->lock);
                free(wq);
                atomic_fetch_add_explicit(&wp->count, -1, memory_order_relaxed);
            }
        } else
            progress += 7;
        progress++;
    }
}

static void mthpc_workpool_init(struct mthpc_workpool *wp, const char *name)
{
    wp->name = name;
    spin_lock_init(&wp->lock);
    mthpc_list_init(&wp->head);
    atomic_init(&wp->count, 0);
}

static void mthpc_workpool_exit(struct mthpc_workpool *wp)
{
    unsigned int count;

    mthpc_synchronize_rcu();
    mthpc_workqueues_join(wp);
    spin_lock_destroy(&wp->lock);
    count = atomic_load(&wp->count);
    MTHPC_WARN_ON(count != 0, "%s workpool might still has workqueue(s)=%u",
                  wp->name, count);
}

// create one thread handle join
static void __mthpc_init mthpc_workqueue_init(void)
{
    mthpc_init_feature();
    mthpc_workpool_init(&mthpc_workpool, "global");
    mthpc_workpool_init(&mthpc_thread_wp, "thread");
    mthpc_workpool_init(&mthpc_taskflow_wp, "taskflow");
    //mthpc_workpool_init(&mthpc_rcu_wp, "rcu");
    /* Add new pool here. */
    mthpc_init_ok();
}

static void __mthpc_exit mthpc_workqueue_exit(void)
{
    mthpc_exit_feature();
    mthpc_workpool_exit(&mthpc_workpool);
    mthpc_workpool_exit(&mthpc_thread_wp);
    mthpc_workpool_exit(&mthpc_taskflow_wp);
    //mthpc_workpool_exit(&mthpc_rcu_wp);
    /* Add new pool here. */
    mthpc_exit_ok();
}
