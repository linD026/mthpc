#include <pthread.h>
#include <stdlib.h>

#include <mthpc/centralized_barrier.h>
#include <mthpc/thread.h>
#include <mthpc/debug.h>
#include <mthpc/util.h>
#include <mthpc/rcu.h>

#define NR_READER 32
#define NR_WRITE 8

static pthread_mutex_t lock;
static int *data;

void read_func(struct mthpc_thread *unused)
{
    int *tmp, val;

    mthpc_rcu_read_lock();
    tmp = mthpc_rcu_deference(data);
    val = *tmp;
    mthpc_rcu_read_unlock();

    mthpc_pr_info("read: number=%d\n", val);

    return;
}

void write_func(struct mthpc_thread *unused)
{
    for (int i = 1; i < NR_WRITE + 1; i++) {
        int *tmp = malloc(sizeof(int));
        MTHPC_BUG_ON(!tmp, "allocation failed");
        *tmp = i;
        pthread_mutex_lock(&lock);
        tmp = mthpc_rcu_replace_pointer(data, tmp);
        pthread_mutex_unlock(&lock);
        mthpc_synchronize_rcu();
        free(tmp);
    }

    return;
}

static MTHPC_DECLARE_THREAD(reader, NR_READER, NULL, read_func, NULL);
static MTHPC_DECLARE_THREAD(writer, 1, NULL, write_func, NULL);

int main(void)
{
    MTHPC_DECLARE_THREADS(threads, &reader, &writer);

    data = malloc(sizeof(int));
    MTHPC_BUG_ON(!data, "allocation failed");
    *data = 0;

    pthread_mutex_init(&lock, NULL);
    mthpc_thread_run(&threads);

    free(data);

    return 0;
}
