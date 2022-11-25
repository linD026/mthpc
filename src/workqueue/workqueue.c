#define _GNU_SOURCE
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#include <mthpc/workqueue.h>
#include <mthpc/debug.h>
#include <list.h>
#include <util.h>

#define MTHPC_WQ_NR_CPU (4U)
#define MTHPC_WQ_CPU_MASK (MTHPC_WQ_NR_CPU - 1)
#define MTHPC_WQ_ACTIVED_FLAG MTHPC_WQ_NR_CPU

struct mthpc_workqueue {
    /* pool uses active to notify the wq should be finished or not. */
    unsigned int __actived_cpu;
    pthread_mutex_t lock;
    pthread_t tid;
    /* the number of work in queue */
    unsigned int count;
    /* workqueue (work) linked list */
    struct mthpc_list_head head;
    /* workpool linked list */
    struct mthpc_list_head node;
};

struct mthpc_workpool {
    pthread_t tid;

    pthread_mutex_t global_lock;
    struct mthpc_list_head global_head;
    /* Use atomic ops to access gpcount even it's protected by lock. */
    unsigned int gpcount;

    //TODO: provide register API
    pthread_mutex_t unique_lock;
    struct mthpc_list_head unique_head;
} __mthpc_aligned__;
static struct mthpc_workpool mthpc_workpool;

static mthpc_always_inline int mthpc_wq_get_cpu(struct mthpc_workqueue *wq)
{
    return (int)(__atomic_load_n(&wq->__actived_cpu, __ATOMIC_RELAXED) &
                 MTHPC_WQ_CPU_MASK);
}

static mthpc_always_inline void mthpc_wq_set_cpu(struct mthpc_workqueue *wq,
                                                 int cpu)
{
    unsigned int masked_cpuid = ~MTHPC_WQ_CPU_MASK | (unsigned int)cpu;

    __atomic_and_fetch(&wq->__actived_cpu, masked_cpuid, __ATOMIC_RELAXED);
}

static mthpc_always_inline void mthpc_wq_run_on_cpu(struct mthpc_workqueue *wq)
{
    unsigned int cpuid = mthpc_wq_get_cpu(wq);
    pthread_t tid = pthread_self();
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(cpuid, &cpuset);

    pthread_setaffinity_np(tid, sizeof(cpuset), &cpuset);
}

static mthpc_always_inline bool mthpc_wq_active(struct mthpc_workqueue *wq)
{
    unsigned int masked_active = 0;

    masked_active = __atomic_load_n(&wq->__actived_cpu, __ATOMIC_ACQUIRE);

    return masked_active & MTHPC_WQ_ACTIVED_FLAG;
}

static mthpc_always_inline void mthpc_wq_mkactive(struct mthpc_workqueue *wq)
{
    __atomic_or_fetch(&wq->__actived_cpu, MTHPC_WQ_ACTIVED_FLAG,
                      __ATOMIC_RELEASE);
}

static mthpc_always_inline void
mthpc_wq_clear_active(struct mthpc_workqueue *wq)
{
    __atomic_and_fetch(&wq->__actived_cpu, ~MTHPC_WQ_ACTIVED_FLAG,
                       __ATOMIC_RELEASE);
}

static struct mthpc_workqueue *mthpc_alloc_workqueue(void)
{
    struct mthpc_workqueue *wq = malloc(sizeof(struct mthpc_workqueue));
    if (!wq)
        return NULL;

    wq->count = 0;
    pthread_mutex_init(&wq->lock, NULL);
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
    unsigned int progress = 0;

    mthpc_wq_run_on_cpu(wq);

    while (1) {
        if (!mthpc_wq_active(wq))
            break;
        if (progress >= 16) {
            progress = 0;
            sched_yield();
        }

        pthread_mutex_lock(&wq->lock);
        if (mthpc_list_empty(&wq->head)) {
            progress += 4;
            MTHPC_WARN_ON(wq->count > 0, "list is empty but count=%u\n",
                          wq->count);
            pthread_mutex_unlock(&wq->lock);
        } else {
            work = container_of(wq->head.next, struct mthpc_work, node);
            mthpc_list_del(&work->node);
            wq->count--;
            pthread_mutex_unlock(&wq->lock);

            /* We shouldn't hold the lock when running work. */
            work->func(work);
            progress++;
        }
    }

    return NULL;
}

static mthpc_always_inline void mthpc_works_handler(struct mthpc_workqueue *wq)
{
    pthread_create(&wq->tid, NULL, mthpc_worker_run, wq);
}

static void inline mthpc_workqueue_add_locked(struct mthpc_workqueue *wq,
                                              struct mthpc_work *work)
{
    /* Prevent mthpc_workqueues_join() to delete the wq. */
    mthpc_list_add_tail(&work->node, &wq->head);
    wq->count++;
    MTHPC_WARN_ON(!mthpc_wq_active(wq), "Add work to inactive wq");
    work->wq = wq;
}

/* get global workqueue */
static struct mthpc_workqueue *mthpc_get_workqueue(int cpu,
                                                   struct mthpc_work *work)
{
    struct mthpc_workqueue *wq = NULL;
    struct mthpc_workqueue *prealloc = NULL;
    struct mthpc_list_head *curr;

    /* fast path - find the existed first. */
    pthread_mutex_lock(&mthpc_workpool.global_lock);
    mthpc_list_for_each (curr, &mthpc_workpool.global_head) {
        struct mthpc_workqueue *tmp =
            container_of(curr, struct mthpc_workqueue, node);
        if (cpu == -1 || (cpu & MTHPC_WQ_CPU_MASK) == mthpc_wq_get_cpu(tmp)) {
            wq = tmp;
            pthread_mutex_lock(&wq->lock);
            mthpc_workqueue_add_locked(wq, work);
            pthread_mutex_unlock(&wq->lock);
            break;
        }
    }
    pthread_mutex_unlock(&mthpc_workpool.global_lock);
    if (wq)
        goto out;

    /* Slow path, step 1 - allocate workqueue */
    prealloc = mthpc_alloc_workqueue();
    if (!prealloc)
        return NULL;
    if (cpu == -1)
        cpu = (sched_getcpu() + 1) & MTHPC_WQ_CPU_MASK;
    mthpc_wq_set_cpu(prealloc, cpu);
    /* We can safely add the work before we add wq to the pool. */
    mthpc_workqueue_add_locked(prealloc, work);

    /* Slow path, step 2 - add to the pool */
    pthread_mutex_lock(&mthpc_workpool.global_lock);
    /* Does others already created? check again */
    mthpc_list_for_each (curr, &mthpc_workpool.global_head) {
        struct mthpc_workqueue *tmp =
            container_of(curr, struct mthpc_workqueue, node);
        if (cpu == mthpc_wq_get_cpu(tmp)) {
            wq = tmp;
            goto unlock;
        }
    }
    /* No one created it, we can safetly add to the pool.*/
    mthpc_list_add_tail(&prealloc->node, &mthpc_workpool.global_head);
    __atomic_fetch_add(&mthpc_workpool.gpcount, 1, __ATOMIC_RELAXED);
    wq = prealloc;
    prealloc = NULL;
unlock:
    pthread_mutex_unlock(&mthpc_workpool.global_lock);
    mthpc_works_handler(wq);

out:
    return wq;
}

/* user API */

int mthpc_schedule_work_on(int cpu, struct mthpc_work *work)
{
    struct mthpc_workqueue *wq;

    mthpc_list_init(&work->node);
    wq = mthpc_get_workqueue(cpu, work);
    if (!wq)
        return -ENOMEM;

    return 0;
}

int mthpc_queue_work(struct mthpc_work *work)
{
    return mthpc_schedule_work_on(-1, work);
}

void mthpc_dump_work(struct mthpc_work *work)
{
    struct mthpc_workqueue *wq = work->wq;

    mthpc_print("dump workqueue(%p) work(%s)\n", wq, work->name);
    mthpc_print("CPU: %u TID: %x\n", mthpc_wq_get_cpu(wq),
                (unsigned int)wq->tid);
    mthpc_print("func: %p private: %p\n", work->func, work->private);
    mthpc_dump_stack();
}

/* init/exit function */

static void mthpc_workqueues_join(struct mthpc_workpool *wp)
{
    struct mthpc_list_head *curr, *n;
    struct mthpc_list_head free_list;
    unsigned int progress = 0;

    while (1) {
        if (!__atomic_load_n(&wp->gpcount, __ATOMIC_RELAXED))
            break;
        if (progress >= 32) {
            progress = 0;
            sched_yield();
            continue;
        }

        mthpc_list_init(&free_list);

        pthread_mutex_lock(&wp->global_lock);
        mthpc_list_for_each_safe (curr, n, &wp->global_head) {
            struct mthpc_workqueue *wq =
                container_of(curr, struct mthpc_workqueue, node);

            pthread_mutex_lock(&wq->lock);
            /* We should waiting for the works in wq. */
            if (!wq->count) {
                mthpc_list_del(&wq->node);
                mthpc_list_add(&wq->node, &free_list);
                mthpc_wq_clear_active(wq);
            }
            pthread_mutex_unlock(&wq->lock);
        }
        pthread_mutex_unlock(&wp->global_lock);

        if (!mthpc_list_empty(&free_list)) {
            mthpc_list_for_each_safe (curr, n, &free_list) {
                struct mthpc_workqueue *wq =
                    container_of(curr, struct mthpc_workqueue, node);

                pthread_join(wq->tid, NULL);
                /*
                 * We don't have to hold lock anymore, however we
                 * should still make sure the read access is atomic ops.
                 */
                MTHPC_WARN_ON(__atomic_load_n(&wq->count, __ATOMIC_RELAXED) > 0,
                              "freeing wq but still holding work(s)");
                pthread_mutex_destroy(&wq->lock);
                free(wq);
                __atomic_fetch_add(&wp->gpcount, -1, __ATOMIC_RELAXED);
            }
        } else
            progress += 7;
        progress++;
    }
}

// create one thread handle join
void mthpc_init(mthpc_indep) mthpc_workqueue_init(void)
{
    pthread_mutex_init(&mthpc_workpool.global_lock, NULL);
    pthread_mutex_init(&mthpc_workpool.unique_lock, NULL);
    mthpc_list_init(&mthpc_workpool.global_head);
    mthpc_list_init(&mthpc_workpool.unique_head);

    mthpc_pr_info("workqueue init\n");
}

void mthpc_exit(mthpc_indep) mthpc_workqueue_exit(void)
{
    mthpc_workqueues_join(&mthpc_workpool);
    pthread_mutex_destroy(&mthpc_workpool.global_lock);
    pthread_mutex_destroy(&mthpc_workpool.unique_lock);
    mthpc_pr_info("workqueue exit\n");
}
