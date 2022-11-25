#include <mthpc/workqueue.h>
#include <mthpc/debug.h>
#include <stdlib.h>

static void dump_work(struct mthpc_work *work)
{
    mthpc_dump_work(work);
}

static MTHPC_DECLARE_WORK(test_work, dump_work, NULL);

int main(void)
{
    mthpc_queue_work(&test_work);
    sleep(2);

    return 0;
}
