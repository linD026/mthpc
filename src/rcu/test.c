#include <pthread.h>
#include <stdlib.h>

#include <mthpc/centralized_barrier.h>
#include <mthpc/debug.h>
#include <mthpc/util.h>
#include <mthpc/rcu.h>

#define NR_READER 32
#define NR_WRITE 8

static MTHPC_DEFINE_BARRIER(barrier);

static pthread_mutex_t lock;
static int *data;

void *read_func(void *unused)
{
    int tmp;

    mthpc_rcu_thread_init();
    mthpc_centralized_barrier(&barrier, NR_READER + 1);

    mthpc_rcu_read_lock();
    tmp = READ_ONCE(*data);
    mthpc_rcu_read_unlock();

    mthpc_pr_info("read: number=%d\n", tmp);

    return NULL;
}

void *write_func(void *unused)
{
    mthpc_rcu_thread_init();
    mthpc_centralized_barrier(&barrier, NR_READER + 1);

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

    return NULL;
}

int main(void)
{
    pthread_t reader[NR_READER];
    pthread_t writer;

    data = malloc(sizeof(int));
    MTHPC_BUG_ON(!data, "allocation failed");
    *data = 0;

    pthread_mutex_init(&lock, NULL);

    for (int i = 0; i < NR_READER; i++)
        pthread_create(&reader[i], NULL, read_func, NULL);
    pthread_create(&writer, NULL, write_func, NULL);

    for (int i = 0; i < NR_READER; i++)
        pthread_join(reader[i], NULL);
    pthread_join(writer, NULL);

    free(data);
}
