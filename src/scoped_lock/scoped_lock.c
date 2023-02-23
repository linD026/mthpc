#include <mthpc/scoped_lock.h>
#include <mthpc/spinlock.h>
#include <mthpc/rcu.h>
#include <mthpc/util.h>
#include <mthpc/debug.h>
#include <mthpc/list.h>

#include <stdatomic.h>

#include <internal/feature.h>
#undef _MTHPC_FEATURE
#define _MTHPC_FEATURE scoped_lock

struct mthpc_sl_data {
    spinlock_t lock;
    struct mthpc_list_head head;
};
static struct mthpc_sl_data mthpc_sl_data;

struct mthpc_sl_node {
    struct mthpc_list_head node;
    struct mthpc_sl_data *data;
    union {
        struct {
            spinlock_t lock;
        };
    };
    unsigned int id;
    unsigned int type;
};

static void mthpc_sl_lock_init(struct mthpc_sl_node *node)
{
    switch (node->type & sl_type_mask) {
    case sl_spin_lock_type:
        spin_lock_init(&node->lock);
        return;
    case sl_rcu_read_lock_type:
        mthpc_rcu_thread_init();
        return;
    default:
        MTHPC_WARN_ON(1, "scoped lock unkown type");
    }
}

static void mthpc_sl_lock_destroy(struct mthpc_sl_node *node)
{
    switch (node->type & sl_type_mask) {
    case sl_spin_lock_type:
        spin_lock_destroy(&node->lock);
        return;
    case sl_rcu_read_lock_type:
        /* Other features may still need rcu. */
        return;
    default:
        MTHPC_WARN_ON(1, "scoped lock unkown type");
    }
}

static void mthpc_insert_scoped_lock(struct mthpc_sl_data *sl_data,
                                     mthpc_scopedlock_t *sl,
                                     mthpc_scopedlock_t *cached)
{
    struct mthpc_list_head *pos;
    struct mthpc_sl_node *node = malloc(sizeof(struct mthpc_sl_node));

    if (!node) {
        MTHPC_WARN_ON(node == NULL, "malloc failed");
        return;
    }

    mthpc_list_init(&node->node);
    node->data = sl_data;
    node->id = sl->id;
    node->type = sl->type;
    atomic_store_explicit(&sl->node, (uintptr_t)node, memory_order_relaxed);

    mthpc_sl_lock_init(node);

    spin_lock(&sl_data->lock);
    mthpc_list_for_each (pos, &sl_data->head) {
        struct mthpc_sl_node *tmp =
            container_of(pos, struct mthpc_sl_node, node);
        if (tmp->id == node->id) {
            MTHPC_WARN_ON(tmp->type != node->type,
                          "id:%d has different type:%d, %d\n", tmp->id,
                          tmp->type, node->type);
            MTHPC_WARN_ON((struct mthpc_sl_node *)atomic_load_explicit(
                              &cached->node, memory_order_relaxed) != tmp,
                          "cached's node, %p, is not the same %p\n",
                          (void *)atomic_load_explicit(&cached->node,
                                                       memory_order_relaxed),
                          tmp);
            free(node);
            atomic_store_explicit(&sl->node, (uintptr_t)tmp,
                                  memory_order_relaxed);
            goto unlock;
        }
    }
    mthpc_list_add_tail(&node->node, &sl_data->head);
    atomic_store_explicit(&cached->node, (uintptr_t)node, memory_order_release);
unlock:
    spin_unlock(&sl_data->lock);
}

void __mthpc_scoped_lock(mthpc_scopedlock_t *sl, mthpc_scopedlock_t *cached)
{
    struct mthpc_sl_node *node;

    if (!sl->node)
        mthpc_insert_scoped_lock(&mthpc_sl_data, sl, cached);

    node = (struct mthpc_sl_node *)atomic_load_explicit(&sl->node,
                                                        memory_order_relaxed);

    switch (sl->type & sl_type_mask) {
    case sl_spin_lock_type:
        spin_lock(&node->lock);
        return;
    case sl_rcu_read_lock_type:
        mthpc_rcu_read_lock();
        return;
    default:
        MTHPC_WARN_ON(1, "scoped lock unkown type");
    }
}

void mthpc_scoped_unlock(mthpc_scopedlock_t *sl)
{
    struct mthpc_sl_node *node = (struct mthpc_sl_node *)atomic_load_explicit(
        &sl->node, memory_order_relaxed);

    switch (sl->type & sl_type_mask) {
    case sl_spin_lock_type:
        spin_unlock(&node->lock);
        return;
    case sl_rcu_read_lock_type:
        mthpc_rcu_read_unlock();
        return;
    default:
        MTHPC_WARN_ON(1, "scoped lock unkown type");
    }
}

static inline void mthpc_sl_data_init(struct mthpc_sl_data *sl_data)
{
    spin_lock_init(&sl_data->lock);
    mthpc_list_init(&sl_data->head);
}

static inline void mthpc_sl_data_exit(struct mthpc_sl_data *sl_data)
{
    struct mthpc_list_head *pos, *n;

    spin_lock(&sl_data->lock);
    mthpc_list_for_each_safe (pos, n, &sl_data->head) {
        struct mthpc_sl_node *tmp =
            container_of(pos, struct mthpc_sl_node, node);
        mthpc_sl_lock_destroy(tmp);
        mthpc_list_del(&tmp->node);
        free(tmp);
    }
    spin_unlock(&sl_data->lock);

    spin_lock_destroy(&sl_data->lock);
}

static void __mthpc_init mthpc_scoped_lock_init(void)
{
    mthpc_init_feature();
    mthpc_sl_data_init(&mthpc_sl_data);
    mthpc_init_ok();
}

static void __mthpc_exit mthpc_scoped_lock_exit(void)
{
    mthpc_exit_feature();
    mthpc_sl_data_exit(&mthpc_sl_data);
    mthpc_exit_ok();
}
