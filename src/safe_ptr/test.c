#include <stdatomic.h>

#include <mthpc/safe_ptr.h>
#include <mthpc/util.h>
#include <mthpc/thread.h>
#include <mthpc/print.h>

#include <stdatomic.h>

struct test {
    atomic_ulong cnt;
};

static void get_and_put(struct mthpc_thread_group *th)
{
    MTHPC_DECLARE_SAFE_PTR(struct test, safe_ptr, th->args);

    atomic_fetch_add_explicit(&safe_ptr->cnt, 1, memory_order_seq_cst);
    printf("cnt=%lu\n",
           atomic_load_explicit(&safe_ptr->cnt, memory_order_consume));
}

static void test_dtor(void *data)
{
    printf("%s: dtor(refcount=%lu)\n", mthpc_safe_proto_of(data)->name,
           atomic_load_explicit(&mthpc_safe_proto_of(data)->refcount,
                                memory_order_consume));
}

static void test_get_and_put(void)
{
    /* Or you can use MTHPC_MAKE_SAFE_PTR(type, name, dtor). */
    MTHPC_DECLARE_SAFE_PTR(struct test, dut,
                           mthpc_unsafe_alloc(struct test, test_dtor));
    MTHPC_DECLARE_THREAD_GROUP(get_and_put_work, 10, NULL, get_and_put, dut);

    mthpc_print("test get and put\n");
    dut->cnt = 0;
    mthpc_thread_async_run(&get_and_put_work);
    mthpc_safe_put(dut);
    mthpc_thread_async_wait(&get_and_put_work);
}

static void borrow_to_here(mthpc_borrow_ptr(struct test) p)
{
    MTHPC_DECLARE_SAFE_PTR_FROM_BORROW(struct test, safe_ptr, p);

    printf("cnt=%lu\n",
           atomic_load_explicit(&safe_ptr->cnt, memory_order_consume));
}

static void test_borrow(void)
{
    MTHPC_MAKE_SAFE_PTR(struct test, dut, test_dtor);

    mthpc_print("test borrow\n");
    atomic_init(&dut->cnt, 0);
    borrow_to_here(mthpc_borrow_to(dut));
}

int main(void)
{
    test_get_and_put();
    test_borrow();

    return 0;
}
