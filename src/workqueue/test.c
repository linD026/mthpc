#include <mthpc/workqueue.h>
#include <mthpc/debug.h>
#include <stdlib.h>
#include <stdio.h>

static void dump_work(struct mthpc_work *work)
{
    printf("heeeeeeee\n");
    mthpc_dump_work(work);
}

static MTHPC_DECLARE_WORK(test_work, dump_work, NULL);

int main(void)
{
    mthpc_printdg("before\n");
    mthpc_queue_work(&test_work);
    sleep(2);
    mthpc_printdg("after\n");

    return 0;
}
