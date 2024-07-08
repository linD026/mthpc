#include <mthpc/safe_ptr.h>
#include <mthpc/util.h>
#include <mthpc/thread.h>
#include <mthpc/print.h>

#include <stdatomic.h>

struct test {
    atomic_ulong cnt;
};

static void test_dtor(void *data)
{
    mthpc_print("call to the test_dtor\n");
}

static void get_and_put(struct mthpc_thread_group *th)
{
    // cb->refcnt_inc
    MTHPC_DECLARE_SAFE_PTR_FROM_BORROW(safe_ptr, th->args);
    struct test *raw_data;

    raw_data = mthpc_safe_ptr_load(&safe_ptr);
    atomic_fetch_add_explicit(&raw_data->cnt, 1, memory_order_seq_cst);
    printf("cnt=%lu\n",
           atomic_load_explicit(&raw_data->cnt, memory_order_consume));
    // cb->refcnt_dec
}

static void test_get_and_put(void)
{
    // cb->refcnt = 1
    MTHPC_DECLARE_SAFE_PTR(dut);
    MTHPC_DECLARE_THREAD_GROUP(get_and_put_work, 10, NULL, get_and_put,
                               mthpc_borrow_safe_ptr(&dut));
    struct test *raw_data = malloc(sizeof(struct test));

    MTHPC_BUG_ON(!raw_data, "malloc");
    raw_data->cnt = 1;

    mthpc_print("test get and put\n");
    mthpc_safe_ptr_store(&dut, raw_data, test_dtor);
    mthpc_thread_async_run(&get_and_put_work);
    mthpc_thread_async_wait(&get_and_put_work);

    // cb->refcnt_dec
}

int main(void)
{
    test_get_and_put();
    // test_lf_stack();

    return 0;
}
