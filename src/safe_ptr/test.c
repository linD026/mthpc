//#include <stdio.h>

#include <mthpc/safe_ptr.h>
#include <mthpc/util.h>
#include <mthpc/thread.h>
#include <mthpc/print.h>

struct test {
    unsigned long cnt;
};

static void get_and_put(struct mthpc_thread *th)
{
    MTHPC_DECLARE_SAFE_PTR(struct test, safe_ptr, th->args);

    __atomic_fetch_add(&safe_ptr->cnt, 1, __ATOMIC_SEQ_CST);
    printf("cnt=%lu\n", READ_ONCE(safe_ptr->cnt));
}

static void test_dtor(void *data)
{
    printf("%s: dtor(refcount=%lu)\n", mthpc_safe_proto_of(data)->name,
           READ_ONCE(mthpc_safe_proto_of(data)->refcount));
}

static void test_get_and_put(void)
{
    struct test *dut = mthpc_safe_alloc(struct test, test_dtor);
    MTHPC_DECLARE_THREAD(get_and_put_work, 10, NULL, get_and_put, dut);

    mthpc_print("test get and put\n");
    dut->cnt = 0;
    mthpc_thread_async_run(&get_and_put_work);
    mthpc_safe_put(dut);
    mthpc_thread_async_wait(&get_and_put_work);
}

static void borrow_to_here(mthpc_borrow_ptr(struct test) p)
{
    MTHPC_DECLARE_SAFE_PTR_FROM_BORROW(struct test, safe_ptr, p);

    printf("cnt=%lu\n", READ_ONCE(safe_ptr->cnt));
}

static void test_borrow(void)
{
    MTHPC_MAKE_SAFE_PTR(struct test, dut, test_dtor);

    mthpc_print("test borrow\n");
    dut->cnt = 0;
    borrow_to_here(mthpc_borrow_to(dut));
}

int main(void)
{
    test_get_and_put();
    test_borrow();

    return 0;
}
