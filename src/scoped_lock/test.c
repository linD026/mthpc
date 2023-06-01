#include <mthpc/scoped_lock.h>
#include <mthpc/thread.h>

static int cnt = 0;
static DEFINE_SPINLOCK(sp);

static void worker(struct mthpc_thread_group *th)
{
    {
        mthpc_scoped_lock(rcu);
        mthpc_scoped_lock(spinlock, &sp);
        cnt++;
    }
}

static MTHPC_DECLARE_THREAD_GROUP(test_work, 10, NULL, worker, NULL);

int main(void)
{
    mthpc_thread_run(&test_work);

    return 0;
}
