#include <stdlib.h>

#include <mthpc/rcu.h>
#include <mthpc/safe_ptr.h>

#include <internal/rcu.h>
#include <internal/feature.h>
#undef _MTHPC_FEATURE
#define _MTHPC_FEATURE safe_ptr

static struct mthpc_rcu_data mthpc_safe_ptr_rcu_data;
static atomic_ulong mthpc_sp_rcu_cnt = 0;

struct mthpc_safe_cb {
    atomic_ulong refcount;
    size_t size;
    void (*dtor)(void *safe_data);
};

#define MTHPC_SAFE_INIT(cb, _size, _dtor) \
    do {                                  \
        (cb)->refcount = 1;               \
        (cb)->size = _size;               \
        (cb)->dtor = _dtor;               \
    } while (0)

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

static inline int mthpc_safe_get(struct mthpc_safe_ptr *sp)
{
    struct mthpc_safe_cb *cb = mthpc_safe_cb_of(sp->p);
    int ret;

    ret = atomic_fetch_add_explicit(&cb->refcount, 1, memory_order_release);

    return ret + 1;
}

static inline int mthpc_safe_put(struct mthpc_safe_ptr *sp)
{
    struct mthpc_safe_cb *cb;
    int ret;

    if (unlikely(!sp))
        return 0;

    cb = mthpc_safe_cb_of(sp->p);
    ret = atomic_fetch_add_explicit(&cb->refcount, -1, memory_order_seq_cst);
    if (ret - 1 == 0) {
        if (cb->dtor)
            cb->dtor(mthpc_safe_data_of(cb));
        free(sp);
        sp->p = NULL;
        return 0;
    }
    return ret;
}

void mthpc_safe_ptr_create(struct mthpc_safe_ptr *stack_sp, size_t size,
                           void (*dtor)(void *))
{
    struct mthpc_safe_cb *cb = NULL;

    cb = malloc(sizeof(struct mthpc_safe_cb) + size);
    MTHPC_WARN_ON(!cb, "malloc");
    if (!cb)
        return;

    cb->refcount = 1;
    cb->size = size;
    cb->dtor = dtor;
    mthpc_rcu_add(&mthpc_safe_ptr_rcu_data,
                  atomic_fetch_add(&mthpc_sp_rcu_cnt, 1), &stack_sp->rcu_node);
    stack_sp->p = cb;
}

// TODO: add multiple type of reclamation

static void mthpc_deferred_reclaim(struct mthpc_safe_ptr *sp)
{
    mthpc_synchronize_rcu_internal(&mthpc_safe_ptr_rcu_data);
    mthpc_safe_put(sp);
}

void mthpc_safe_ptr_store(struct mthpc_safe_ptr *sp, void *new)
{
}

void *mthpc_safe_ptr_load(struct mthpc_safe_ptr *sp)
{
    mthpc_rcu_read_lock_internal(sp->rcu_node);
    mthpc_rcu_read_unlock_internal(sp->rcu_node);
}

void *mthpc_safe_ptr_cmpxhg(struct mthpc_safe_ptr *sp, void *new)
{
}

struct mthpc_safe_ptr __mthpc_brw *mthpc_borrow_to(struct mthpc_safe_ptr *sp)
{
    mthpc_safe_get(sp);
    return (struct mthpc_safe_ptr __mthpc_brw *)sp;
}

static __mthpc_init void mthpc_safe_ptr_init(void)
{
    mthpc_init_feature();
    mthpc_rcu_data_init(&mthpc_safe_ptr_rcu_data, MTHPC_RCU_USR);
    mthpc_init_ok();
}

static __mthpc_exit void mthpc_safe_ptr_exit(void)
{
    mthpc_exit_feature();
    mthpc_exit_ok();
}
