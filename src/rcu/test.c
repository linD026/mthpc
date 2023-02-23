#include <pthread.h>
#include <stdlib.h>

#include <mthpc/centralized_barrier.h>
#include <mthpc/thread.h>
#include <mthpc/debug.h>
#include <mthpc/util.h>
#include <mthpc/rcu.h>

#define NR_READER 32
#define NR_WRITE 5

static pthread_mutex_t lock;
static int *data;

void read_func(struct mthpc_thread_group *unused)
{
    int *tmp, val;

    pthread_mutex_lock(&lock);
    pthread_mutex_unlock(&lock);
    mthpc_rcu_read_lock();
    tmp = mthpc_rcu_dereference(data);
    val = *tmp;
    //printf("val=%d\n", val);
    mthpc_rcu_read_unlock();

    return;
}

void write_init(struct mthpc_thread_group *th)
{
    int **arr;

    arr = malloc(sizeof(int *) * NR_WRITE);
    MTHPC_BUG_ON(!arr, "malloc");
    for (int i = 0; i < NR_WRITE; i++) {
        arr[i] = malloc(sizeof(int));
        MTHPC_BUG_ON(!arr[i], "malloc");
        *arr[i] = i + 1;
    }

    th->args = arr;
}

void write_func(struct mthpc_thread_group *th)
{
    int *old, *tmp, **arr = th->args;

    for (int i = 0; i < NR_WRITE; i++) {
        tmp = arr[i];
        pthread_mutex_lock(&lock);
        old = mthpc_rcu_replace_pointer(data, tmp);
        pthread_mutex_unlock(&lock);
        mthpc_synchronize_rcu();
        free(old);
    }

    free(arr);

    return;
}

static MTHPC_DECLARE_THREAD_GROUP(reader, NR_READER, NULL, read_func, NULL);
static MTHPC_DECLARE_THREAD_GROUP(writer, 1, write_init, write_func, NULL);

int main(void)
{
    MTHPC_DECLARE_THREAD_CLUSTER(threads, &reader, &writer);

    mthpc_rcu_read_lock();
    mthpc_rcu_read_unlock();

    data = malloc(sizeof(int));
    MTHPC_BUG_ON(!data, "allocation failed");
    *data = 0;

    pthread_mutex_init(&lock, NULL);
    mthpc_thread_run(&threads);

    free(data);

    return 0;
}
