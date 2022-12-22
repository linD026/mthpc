#include <stdio.h>

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
    mthpc_thread_run(&get_and_put_work);
}

static void borrow_to_here(struct test __mthpc_borrowed *unsafe_ptr)
{
    MTHPC_DECLARE_SAFE_PTR(struct test, safe_ptr, unsafe_ptr);

    printf("cnt=%lu\n", READ_ONCE(safe_ptr->cnt));
}

static void test_borrow(void)
{
    MTHPC_DECLARE_SAFE_DATA(struct test, dut, test_dtor);

    mthpc_print("test borrow\n");
    dut->cnt = 0;
    borrow_to_here(mthpc_borrow(dut));
}

int main(void)
{
    test_get_and_put();
    test_borrow();

    return 0;
}
