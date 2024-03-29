#include <mthpc/workqueue.h>
#include <mthpc/rcu.h>
#include <mthpc/debug.h>
#include <unistd.h>

static void dump_work(struct mthpc_work *work);
static MTHPC_DECLARE_WORK(test_work, dump_work, NULL);

static int cnt = 0;

static void dump_work(struct mthpc_work *work)
{
    mthpc_dump_work(work);
    if (cnt == 10) {
        return;
    }
    mthpc_pr_info("cnt=%d\n", cnt++);
    mthpc_schedule_work_on(cnt, work);
}

int main(void)
{
    //mthpc_rcu_thread_init();
    mthpc_queue_work(&test_work);

    return 0;
}
