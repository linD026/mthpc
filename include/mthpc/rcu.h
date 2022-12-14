#ifndef __MTHPC_RCU_H__
#define __MTHPC_RCU_H__

#include <mthpc/spinlock.h>
#include <mthpc/debug.h>
#include <mthpc/util.h>

/*
 * the use case in mthpc library would be like this.
 *
 * struct my_node {
 *     struct mthpc_rcu_node node;
 *     ...
 *     struct my_node *next;
 *
 * };
 *
 * struct my_head {
 *     struct mthpc_rcu_data head;
 *     ...
 *     struct my_node *head;
 * };
 *
 * The count will change to protect per object rather
 * than thread from user rcu_data.
 */

struct mthpc_rcu_node {
    unsigned long id;
    unsigned long gp_seq;
    struct mthpc_rcu_node *next;
    struct mthpc_rcu_data *data;
} __mthpc_aligned__;

struct mthpc_rcu_data {
    spinlock_t lock;
    struct mthpc_rcu_node *head;
    unsigned int type;
    unsigned long gp_seq;

    struct mthpc_rcu_data *next;
};

extern __thread struct mthpc_rcu_node *mthpc_rcu_node_ptr;

#define MTHPC_GP_COUNT (1UL << 0)
#define MTHPC_GP_CTR_PHASE (1UL << (sizeof(unsigned long) << 2))
#define MTHPC_GP_CTR_NEST_MASK (MTHPC_GP_CTR_PHASE - 1)

void mthpc_rcu_thread_init(void);
void mthpc_rcu_thread_exit(void);

static __always_inline void __allow_unused
mthpc_rcu_read_lock_internal(struct mthpc_rcu_node *node)
{
    unsigned long node_gp = 0;

    MTHPC_WARN_ON(!node, "rcu node == NULL");
    mthpc_cmb();

    node_gp = READ_ONCE(node->gp_seq);
    if (likely(!(node_gp & MTHPC_GP_CTR_NEST_MASK))) {
        __atomic_store_n(&node->gp_seq, READ_ONCE(node->data->gp_seq),
                         __ATOMIC_SEQ_CST);
    } else
        __atomic_fetch_add(&node->gp_seq, MTHPC_GP_COUNT, __ATOMIC_RELAXED);
}

static __always_inline void __allow_unused
mthpc_rcu_read_unlock_internal(struct mthpc_rcu_node *node)
{
    unsigned long node_gp = 0;

    MTHPC_WARN_ON(!node, "rcu node == NULL");
    mthpc_cmb();
    node_gp = READ_ONCE(node->gp_seq);
    if (likely((node_gp & MTHPC_GP_CTR_NEST_MASK) == MTHPC_GP_COUNT)) {
        __atomic_fetch_add(&node->gp_seq, -MTHPC_GP_COUNT, __ATOMIC_SEQ_CST);
    } else
        __atomic_fetch_add(&node->gp_seq, -MTHPC_GP_COUNT, __ATOMIC_RELAXED);
}

static __always_inline void mthpc_rcu_read_lock(void)
{
    if (unlikely(!mthpc_rcu_node_ptr))
        mthpc_rcu_thread_init();
    mthpc_rcu_read_lock_internal(mthpc_rcu_node_ptr);
}

static __always_inline void mthpc_rcu_read_unlock(void)
{
    mthpc_rcu_read_unlock_internal(mthpc_rcu_node_ptr);
}

void mthpc_synchronize_rcu(void);

#define mthpc_rcu_replace_pointer(p, new) \
    ({ __atomic_exchange_n(&(p), (new), __ATOMIC_RELEASE); })

#define mthpc_rcu_dereference(p) ({ __atomic_load_n(&(p), __ATOMIC_CONSUME); })

void mthpc_synchronize_rcu_all(void);

struct mthpc_rcu_head;

void mthpc_call_rcu(struct mthpc_rcu_head *head,
                    void (*func)(struct mthpc_rcu_head *));

#endif /* __MTHPC_RCU_H__ */
