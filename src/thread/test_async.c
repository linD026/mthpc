#include <mthpc/thread.h>
#include <mthpc/print.h>

static void thread_a(struct mthpc_thread *th)
{
    static int local_cnt = 0;

    if (++local_cnt == 2)
        *(int *)th->args = 1;

    mthpc_pr_info("thread A called:%d\n", local_cnt);

    return;
}

static void thread_b(struct mthpc_thread *th)
{
    static int local_cnt = 0;

    local_cnt++;
    mthpc_pr_info("thread B called:%d\n", local_cnt);

    return;
}

static int th_arg_a = 0;

static MTHPC_DECLARE_THREAD(th_obj_a, 1, thread_a, NULL, &th_arg_a);
static MTHPC_DECLARE_THREAD(th_obj_b, 1, thread_b, NULL, NULL);
static MTHPC_DECLARE_THREADS(thg_obj, &th_obj_a, &th_obj_b);

int main(void)
{
    /* Run thread group object */
    mthpc_thread_async_run(&thg_obj);

    mthpc_pr_info("th_arg_a=%d\n", th_arg_a);

    return 0;
}
