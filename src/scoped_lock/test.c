#include <mthpc/scoped_lock.h>
#include <mthpc/thread.h>

static int cnt = 0;

static void worker(struct mthpc_thread *th)
{
    {
        mthpc_scoped_lock(spin_lock);
        cnt++;
    }
}

static MTHPC_DECLARE_THREAD(test_work, 10, NULL, worker, NULL);

int main(void)
{
    mthpc_thread_run(&test_work);

    return 0;
}
