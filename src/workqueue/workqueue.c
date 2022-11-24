#define _GNU_SOURCE
#include <pthread.h>

#include <mthpc/workqueue.h>
#include <mthpc/debug.h>
#include <list.h>
#include <util.h>

#define MTHPC_WQ_NR_CPU (4U)
#define MTHPC_WQ_CPU_MASK (MTHPC_WQ_NR_CPU - 1)
#define MTHPC_WQ_ACTIVED_FLAG MTHPC_WQ_CPU_MASK

struct mthpc_workqueue {
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
    pthread_mutex_t global_lock;
    struct mthpc_list_head global_head;
    unsigned int count;

    //TODO: provide register API
    pthread_mutex_t unique_lock;
    struct mthpc_list_head unique_head;
} __mthpc_aligned__;
struct mthpc_workpool mthpc_workpool;

static mthpc_always_inline int mthpc_wq_get_cpu(struct mthpc_workqueue *wq)
{
    return (int)(__atomic_load_n(&wq->__actived_cpu, __ATOMIC_RELAXED) &
                 MTHPC_WQ_CPU_MASK);
}

static mthpc_always_inline void mthpc_wq_set_cpu(struct mthpc_workqueue *wq,
                                                 int cpu)
{
    unsigned int masked_cpuid = ~MTHPC_WQ_CPU_MASK | (unsigned int)cpu;

    __atomic_or_fetch(&wq->__actived_cpu, masked_cpuid, __ATOMIC_RELAXED);
}

static mthpc_always_inline void mthpc_wq_run_on_cpu(struct mthpc_workqueue *wq)
{
    unsigned int cpuid = mthpc_wq_get_cpu(wq);
    pthread_t tid = pthread_self();
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(cpuid, &cpuset);

    if (pthread_setaffinity_np(tid, sizeof(cpuset), &cpuset))
        return;
    __atomic_or_fetch(&wq->__actived_cpu, masked_cpuid, __ATOMIC_RELAXED);
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
    /* we active the wq when starting the thread. */
    /* we set the wq to its cpu when running the thread. */

    return wq;
}

static int mthpc_worker_run(void *arg)
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
        } else {
            wp->count--;
            work = wq->head.next;
            mthpc_list_del(&work->node);
            work->func(work);
            progress++;
        }
        pthread_mutex_unlock(&wq->lock);
    }

    return 0;
}

static mthpc_always_inline void mthpc_works_handler(struct mthpc_workqueue *wq)
{
    if (!mthpc_wq_active(wq)) {
        mthpc_wq_mkactive(wq);
        pthread_create(&wq->tid, NULL, mthpc_worker_run, wq);
    }
}

/* get global workqueue */
static struct mthpc_workqueue *mthpc_get_workqueue(int cpu)
{
    struct mthpc_workqueue *wq = NULL;
    struct mthpc_workqueue *prealloc = NULL;
    struct mthpc_list_head *curr;

    /* fast path - find the existed first. */
    pthread_mutex_lock(&mthpc_workpool.global_lock);
    mthpc_list_for_each (curr, &wq->globacl_head) {
        struct mthpc_workqueue *tmp =
            container_of(curr, struct mthpc_workqueue, node);
        if (cpu == -1 || cpu == mthpc_wq_get_cpu()) {
            wq = tmp;
            break;
        }
    }
    pthread_mutex_unlock(&mthpc_workpool.global_lock);
    if (wq)
        goto out;

    /* Slow path, step 1 - allocate workqueue */
    prealloc = mthpc_alloc_workqueue();
    if (!wq)
        return NULL;
    mthpc_wq_mkactive(prealloc);
    mthpc_wq_set_cpu(prealloc, cpu);

    /* Slow path, step 2 - add to the pool */
    pthread_mutex_lock(&mthpc_workpool.global_lock);
    /* Does others already created? check again */
    mthpc_list_for_each (curr, &wq->globacl_head) {
        struct mthpc_workqueue *tmp =
            container_of(curr, struct mthpc_workqueue, node);
        if (cpu == mthpc_wq_get_cpu()) {
            wq = tmp;
            goto unlock;
        }
    }
    /* No one created it, we can safetly add to the pool.*/
    mthpc_list_add_tail(&prealloc->node, &mthpc_workpool.global_head);
    wq = prealloc;
    prealloc = NULL;
unlock:
    pthread_mutex_unlock(&mthpc_workpool.global_lock);

out:
    return wq;
}

/* user API */

int mthpc_schedule_work_on(int cpu, struct mthpc_work *work)
{
    struct mthpc_workqueue *wq = mthpc_get_workqueue(cpu);
    if (!wq)
        return -ENOMEM;

    pthread_mutex_lock(&wq->lock);
    mthpc_list_add_tail(&work->node, &wq->head);
    wq->count++;
    pthread_mutex_unlock(&wq->lock);

    mthpc_works_handler(wq);

    return 0;
}

int mthpc_queue_work(struct mthpc_work *work)
{
    return mthpc_schedule_work_on(-1, work);
}

mthpc_noinline void mthpc_dump_work(struct mthpc_work *work)
{
    printdg();
}

/* init/exit function */

static void mthpc_workqueues_join_handler(void *arg)
{
    struct mthpc_workpool *wp = arg;
}

// create one thread handle join
void mthpc_init mthpc_workqueue_init(void)
{
    // exit = false;
    pthread_create();
}

void mthpc_exit mthpc_workqueue_exit(void)
{
    // exit = true;
    pthread_join();
}
