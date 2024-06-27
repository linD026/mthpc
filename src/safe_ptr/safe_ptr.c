#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <stdint.h>

#include <mthpc/rcu.h>
#include <mthpc/safe_ptr.h>

#include <internal/rcu.h>
#include <internal/feature.h>
#undef _MTHPC_FEATURE
#define _MTHPC_FEATURE safe_ptr

/*
 * # Design
 *
 *
 *  +----------------+         +----------+
 *  | control block  |         | safe_ptr |
 *  +----------------+  sync   +----------+
 *  |    rcu_data    |---------| rcu_node |------ ...
 *  |    refcount    |         +----------+
 *  | protected ptr  |<-+            |
 *  +----------------+  |            |
 *                      |          [user]
 *               access |            |
 *                      +----< safe ptr APIs >
 *
 *
 * safe_ptr sp = make_safe_ptr(type, context, dtor);
 * T data = safe_ptr_load(sp);
 * safe_ptr_store(sp, data);
 *
 * The target of the safe ptr is to provide the lock-free and memory safe
 * smart pointer interface.
 *
 * The user will use the struct mthpc_safe_ptr structure to manage the
 * pointer they want to protect. And, use the store, load, cmpxchg, etc
 * operations to access the protected pointer.
 *
 * # Ownership
 * 
 * We mange the safe_ptr's lifetime with the function scope range. We see
 * the new created safe_ptr in current function stack as local safe_ptr. 
 * Whenever, the user want to use the local safe_ptr in other function stack,
 * the user should use borrow interface to pass the local safe_ptr to the
 * target function.
 *
 * # Lifetime
 *
 * Inside the safe_ptr, we use the control block and its refcount to manage
 * the object we protected. If the refcount is eqaul to zero we will do the
 * reclamation.
 *
 * For each protected object, we use the per-object rcu synchronization
 * to avoid the use-after-free bug. However, we don't directly use rcu
 * to prectect the object, instead, we use rcu to prevent the other thread
 * free the control block during the duplication on read side.
 *
 * # Future work
 *
 * In the real workload, we expect every thread has the progress which
 * we need to have wait-free safe_ptr instead of lock-free one.
 * But now, since we will reclaim the object on the write side, the
 * write side thread might be block by the read side.
 * To improve this, first we have to rewrite the mthpc RCU basic mechanism
 * since we use spinlock to protect the traveral of rcu data checking.
 * After that, we can start to rethink the wait-free safe_ptr for the write
 * operation.
 */

static atomic_ulong mthpc_sp_rcu_cnt = 0;

struct mthpc_safe_cb {
    atomic_ulong refcount;
    struct mthpc_rcu_data rcu_data;
    size_t size;
    void (*dtor)(void *safe_data);
};

#define MTHPC_SAFE_INIT(cb, _size, _dtor) \
    do {                                  \
        (cb)->refcount = 1;               \
        (cb)->size = _size;               \
        (cb)->dtor = _dtor;               \
    } while (0)

static inline struct mthpc_safe_cb *mthpc_cb_load(struct mthpc_safe_ptr *sp)
{
    return (struct mthpc_safe_cb *)atomic_load_explicit(&sp->cb,
                                                        memory_order_acquire);
}

static inline void mthpc_cb_store(struct mthpc_safe_ptr *sp,
                                  struct mthpc_safe_cb *desired)
{
    atomic_store_explicit(&sp->cb, (uintptr_t)desired, memory_order_release);
}

static inline void mthpc_cb_xchg(struct mthpc_safe_ptr *sp,
                                 struct mthpc_safe_cb **desired)
{
    atomic_store_explicit(&sp->cb, (uintptr_t)*desired, memory_order_release);
}

static inline void *mthpc_safe_data_of(struct mthpc_safe_cb *cb)
{
    MTHPC_BUG_ON(!cb, "cb == NULL");
    return (void *)((char *)cb + sizeof(struct mthpc_safe_cb));
}

static inline struct mthpc_safe_cb *mthpc_safe_cb_of(void *safe_data)
{
    MTHPC_BUG_ON(!safe_data, "safe_data == NULL");
    return (struct mthpc_safe_cb *)((char *)safe_data -
                                    sizeof(struct mthpc_safe_cb));
}

static int mthpc_cb_get(struct mthpc_safe_cb *cb)
{
    int ret;

    ret = atomic_fetch_add_explicit(&cb->refcount, 1, memory_order_release);
    MTHPC_WARN_ON(ret == 0, "refcount is zero");

    mthpc_pr_info("sp cb refcnt:%d\n", ret + 1);

    return ret + 1;
}

static int mthpc_cb_put(struct mthpc_safe_cb *cb)
{
    int ret;

    ret = atomic_fetch_add_explicit(&cb->refcount, -1, memory_order_seq_cst);
    MTHPC_WARN_ON(ret < 1, "refcount is corrupted (refcnt:%d)", ret);
    if (ret == 1) {
        // TODO: add multiple type of reclamation
        /* Wait unitl all the read finish the duplication */
        mthpc_synchronize_rcu_internal(&cb->rcu_data);
        if (cb->dtor)
            cb->dtor(mthpc_safe_data_of(cb));
    }
    return ret - 1;
}

static int mthpc_safe_get(struct mthpc_safe_ptr *sp)
{
    struct mthpc_safe_cb *cb;

    if (MTHPC_WARN_ON(!sp, "sp == NULL"))
        return 0;

    cb = mthpc_cb_load(sp);
    return mthpc_cb_get(cb);
}

// TODO: omit this unused function
static __allow_unused int mthpc_safe_put(struct mthpc_safe_ptr *sp)
{
    int ret;
    struct mthpc_safe_cb *cb;

    if (MTHPC_WARN_ON(!sp, "sp == NULL"))
        return 0;

    cb = mthpc_cb_load(sp);
    ret = mthpc_cb_put(cb);
    if (!ret)
        mthpc_cb_store(sp, NULL);

    return ret;
}

static void mthpc_safe_ptr_add(struct mthpc_safe_ptr *new,
                               struct mthpc_safe_cb *cb)
{
    // TODO: we should avoid to use the single atomic counter for the rcu id
    mthpc_rcu_add(&cb->rcu_data, new->id, &new->rcu_node);
    mthpc_cb_store(new, cb);
}

__allow_unused void mthpc_dump_safe_ptr(struct mthpc_safe_ptr *sp)
{
    struct mthpc_safe_cb *cb = NULL;
    unsigned long cnt;
    size_t size;
    void *dtor, *rcu_data;

    mthpc_print("Safe ptr dump: %px, id: %lu, rcu node: %px\n", sp, sp->id,
                sp->rcu_node);

    mthpc_rcu_read_lock_internal(sp->rcu_node);
    cb = mthpc_cb_load(sp);
    cnt = atomic_load_explicit(&cb->refcount, memory_order_acquire);
    size = cb->size;
    dtor = (void *)cb->dtor;
    rcu_data = (void *)&cb->rcu_data;
    mthpc_rcu_read_unlock_internal(sp->rcu_node);

    mthpc_print(
        "Control block: %px, refcount: %lu rcu data: %px object size:%zu, object dtor: %px\n",
        cb, cnt, rcu_data, size, dtor);
}

void mthpc_safe_ptr_create(struct mthpc_safe_ptr *sp, size_t size,
                           void (*dtor)(void *))
{
    struct mthpc_safe_cb *cb = NULL;

    cb = malloc(sizeof(struct mthpc_safe_cb) + size);
    MTHPC_WARN_ON(!cb, "malloc");
    if (!cb)
        return;

    /* control block */
    cb->refcount = 1;
    cb->size = size;
    cb->dtor = dtor;
    mthpc_rcu_data_init(&cb->rcu_data, MTHPC_RCU_USR);

    /* safe_ptr */
    sp->id = atomic_fetch_add(&mthpc_sp_rcu_cnt, 1);
    mthpc_safe_ptr_add(sp, cb);
}

void mthpc_safe_ptr_destroy(struct mthpc_safe_ptr *sp)
{
    struct mthpc_safe_cb *cb = NULL;

    cb = mthpc_cb_load(sp);
    mthpc_rcu_del(&cb->rcu_data, sp->id, sp->rcu_node);
    mthpc_cb_put(cb);
}

// TODO: might need to remove this API
void mthpc_safe_ptr_store_sp(struct mthpc_safe_ptr *sp, void *new_safe_data)
{
    struct mthpc_safe_cb *new_cb = mthpc_safe_cb_of(new_safe_data);

    mthpc_rcu_read_lock_internal(sp->rcu_node);
    mthpc_cb_xchg(sp, &new_cb);
    mthpc_rcu_read_unlock_internal(sp->rcu_node);
    if (new_cb)
        mthpc_cb_put(new_cb);
}

void __mthpc_safe_ptr_store_data(struct mthpc_safe_ptr *sp, void *raw_data,
                                 size_t size)
{
    struct mthpc_safe_cb *cb;

    mthpc_rcu_read_lock_internal(sp->rcu_node);
    cb = mthpc_cb_load(sp);
    memcpy(mthpc_safe_data_of(cb), raw_data, size);
    mthpc_rcu_read_unlock_internal(sp->rcu_node);
}

void mthpc_safe_ptr_load(struct mthpc_safe_ptr *sp, void *dst)
{
    unsigned long *ret = dst;
    struct mthpc_safe_cb *cb = NULL;

    mthpc_rcu_read_lock_internal(sp->rcu_node);
    cb = mthpc_cb_load(sp);
    memcpy(ret, (unsigned long *)mthpc_safe_data_of(cb), cb->size);
    mthpc_rcu_read_unlock_internal(sp->rcu_node);
}

int mthpc_safe_ptr_cmpxhg(struct mthpc_safe_ptr *sp, void *expected,
                          void *desired)
{
    return 0;
}

struct mthpc_safe_ptr *
mthpc_pass_sp_post(struct mthpc_safe_ptr __mthpc_brw *pass_sp,
                   struct mthpc_safe_ptr *sp)
{
    struct mthpc_safe_ptr *unsafe_sp = disenchant_ptr(pass_sp);

    mthpc_safe_get(unsafe_sp);
    sp->id = atomic_fetch_add(&mthpc_sp_rcu_cnt, 1);
    mthpc_safe_ptr_add(sp, mthpc_cb_load(unsafe_sp));
    return sp;
}

static __mthpc_init void mthpc_safe_ptr_init(void)
{
    mthpc_init_feature();
    mthpc_init_ok();
}

static __mthpc_exit void mthpc_safe_ptr_exit(void)
{
    mthpc_exit_feature();
    mthpc_exit_ok();
}
