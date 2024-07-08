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

struct mthpc_safe_cb {
    atomic_ulong refcount;
    struct mthpc_rcu_data rcu_data;
    atomic_uintptr_t raw_ptr;
    void (*dtor)(void *safe_data);
};

struct mthpc_cb_retire_obj {
    void (*lock)(struct mthpc_safe_ptr *sp);
    void (*unlock)(struct mthpc_safe_ptr *sp);
    void (*sp_init)(struct mthpc_safe_ptr *sp, struct mthpc_safe_cb *cb);
    void (*synchronize)(struct mthpc_safe_cb *cb);
};

static struct mthpc_cb_retire_obj mthpc_cb_retire_obj;

static atomic_ulong mthpc_sp_rcu_cnt = 0;

/* Retire related functions */

static void mthpc_cb_rcu_lock(struct mthpc_safe_ptr *sp)
{
    mthpc_rcu_read_lock_internal(sp->rcu_node);
}

static void mthpc_cb_rcu_unlock(struct mthpc_safe_ptr *sp)
{
    mthpc_rcu_read_unlock_internal(sp->rcu_node);
}

static void mthpc_cb_rcu_init(struct mthpc_safe_ptr *sp,
                              struct mthpc_safe_cb *cb)
{
    // TODO: we should avoid to use the single atomic counter for the rcu id
    mthpc_rcu_add(&cb->rcu_data, sp->id, &sp->rcu_node);
}

static void mthpc_cb_synchronize_rcu(struct mthpc_safe_cb *cb)
{
    mthpc_synchronize_rcu_internal(&cb->rcu_data);
    mthpc_rcu_data_exit(&cb->rcu_data);
}

/* Control block related functions */

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

static inline struct mthpc_safe_cb *mthpc_cb_xchg(struct mthpc_safe_ptr *sp,
                                                  struct mthpc_safe_cb *desired)
{
    return (struct mthpc_safe_cb *)atomic_exchange_explicit(
        &sp->cb, (uintptr_t)desired, memory_order_release);
}

static int mthpc_cb_get(struct mthpc_safe_cb *cb)
{
    int ret;

    ret = atomic_fetch_add_explicit(&cb->refcount, 1, memory_order_release);
    MTHPC_WARN_ON(ret == 0, "refcount is zero");

    return ret + 1;
}

static int mthpc_cb_put(struct mthpc_safe_cb *cb, struct mthpc_safe_ptr *old)
{
    int ret;

    ret = atomic_fetch_add_explicit(&cb->refcount, -1, memory_order_seq_cst);
    MTHPC_WARN_ON(ret < 1, "refcount is corrupted (refcnt:%d)", ret);
    if (ret == 1) {
        /* Wait unitl all the reader finish its operations */
        mthpc_cb_retire_obj.synchronize(cb);
        if (cb->dtor)
            cb->dtor((unsigned long *)atomic_load(&cb->raw_ptr));
        else
            free((unsigned long *)atomic_load(&cb->raw_ptr));
    } else {
        mthpc_rcu_del(&cb->rcu_data, old->id, old->rcu_node);
    }

    return ret - 1;
}

static inline struct mthpc_safe_cb *mthpc_cb_create(void *__new_data,
                                                    void (*dtor)(void *))
{
    uintptr_t new_data = (uintptr_t)__new_data;
    struct mthpc_safe_cb *cb = NULL;

    cb = malloc(sizeof(struct mthpc_safe_cb));
    MTHPC_WARN_ON(!cb, "malloc");
    if (!cb)
        return NULL;

    /* control block */
    cb->refcount = 1;
    atomic_store_explicit(&cb->raw_ptr, new_data, memory_order_relaxed);
    cb->dtor = dtor;
    mthpc_rcu_data_init(&cb->rcu_data, MTHPC_RCU_USR);

    return cb;
}

/* user APIs */

__allow_unused void mthpc_dump_safe_ptr(struct mthpc_safe_ptr *sp)
{
    struct mthpc_safe_cb *cb = NULL;
    unsigned long cnt;
    void *dtor, *rcu_data;

    mthpc_print("Safe ptr dump: %px, id: %lu, rcu node: %px\n", sp, sp->id,
                sp->rcu_node);

    mthpc_cb_retire_obj.lock(sp);
    cb = mthpc_cb_load(sp);
    cnt = atomic_load_explicit(&cb->refcount, memory_order_acquire);
    dtor = (void *)cb->dtor;
    rcu_data = (void *)&cb->rcu_data;
    mthpc_cb_retire_obj.unlock(sp);

    mthpc_print(
        "Control block: %px, refcount: %lu rcu data: %px raw pointer:%px, object dtor: %px\n",
        cb, cnt, rcu_data, (unsigned long *)atomic_load(&cb->raw_ptr), dtor);
}

void mthpc_safe_ptr_create(struct mthpc_safe_ptr *sp, void *new_data,
                           void (*dtor)(void *))
{
    struct mthpc_safe_cb *cb = NULL;

    cb = mthpc_cb_create(new_data, dtor);

    /* safe_ptr */
    sp->id = atomic_fetch_add(&mthpc_sp_rcu_cnt, 1);
    mthpc_cb_retire_obj.sp_init(sp, cb);
    mthpc_cb_store(sp, cb);
}

void mthpc_safe_ptr_destroy(struct mthpc_safe_ptr *sp)
{
    struct mthpc_safe_cb *cb = NULL;

    cb = mthpc_cb_load(sp);
    // FIXME: we should remove the rcu_node after exit
    mthpc_cb_put(cb, sp);
}

void mthpc_safe_ptr_store(struct mthpc_safe_ptr *sp, void *new_data,
                          void (*dtor)(void *))
{
    struct mthpc_safe_cb *new_cb, *old_cb;

    /* Prepare the new cb. */
    new_cb = mthpc_cb_create(new_data, dtor);

    old_cb = mthpc_cb_xchg(sp, new_cb);
    if (!old_cb) {
        /*
         * After update the control block pointer in sp,
         * we need to clean up the relationship between sp and old cb.
         */
        mthpc_cb_put(old_cb, sp);
    }
    /* After clean up the old relationship, let's build a new one. */
    mthpc_cb_retire_obj.sp_init(sp, new_cb);
}

void *mthpc_safe_ptr_load(struct mthpc_safe_ptr *sp)
{
    unsigned long *ret = NULL;
    struct mthpc_safe_cb *cb = NULL;

    mthpc_cb_retire_obj.lock(sp);
    cb = mthpc_cb_load(sp);
    ret = (unsigned long *)atomic_load_explicit(&cb->raw_ptr,
                                                memory_order_acquire);
    mthpc_cb_retire_obj.unlock(sp);

    return ret;
}

int mthpc_safe_ptr_cmpxhg(struct mthpc_safe_ptr *sp, void **expected,
                          void *desired)
{
    struct mthpc_safe_cb *cb;
    int ret = 0;

    mthpc_cb_retire_obj.lock(sp);
    cb = mthpc_cb_load(sp);
    ret = atomic_compare_exchange_strong(&cb->raw_ptr, (uintptr_t *)expected,
                                         (uintptr_t)desired);
    mthpc_cb_retire_obj.unlock(sp);

    return ret;
}

struct mthpc_safe_ptr *
mthpc_pass_sp_post(struct mthpc_safe_ptr __mthpc_brw *pass_sp,
                   struct mthpc_safe_ptr *sp)
{
    struct mthpc_safe_ptr *unsafe_sp = disenchant_ptr(pass_sp);
    struct mthpc_safe_cb *cb = NULL;

    mthpc_cb_retire_obj.lock(sp);
    cb = mthpc_cb_load(unsafe_sp);

    mthpc_cb_get(cb);
    sp->id = atomic_fetch_add(&mthpc_sp_rcu_cnt, 1);
    mthpc_cb_retire_obj.sp_init(sp, cb);
    mthpc_cb_store(sp, cb);
    mthpc_cb_retire_obj.unlock(sp);

    return sp;
}

/* init/exit functions */

static __mthpc_init void mthpc_safe_ptr_init(void)
{
    mthpc_init_feature();
    // TODO: provide config for multi-type of reclamation
    mthpc_cb_retire_obj.lock = mthpc_cb_rcu_lock;
    mthpc_cb_retire_obj.unlock = mthpc_cb_rcu_unlock;
    mthpc_cb_retire_obj.sp_init = mthpc_cb_rcu_init;
    mthpc_cb_retire_obj.synchronize = mthpc_cb_synchronize_rcu;
    mthpc_init_ok();
}

static __mthpc_exit void mthpc_safe_ptr_exit(void)
{
    mthpc_exit_feature();
    mthpc_exit_ok();
}
