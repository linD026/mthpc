#include <stdbool.h>
#include <stdlib.h>
#include <stdatomic.h>

#include <mthpc/rcu.h>
#include <mthpc/spinlock.h>
#include <mthpc/debug.h>
#include <mthpc/util.h>

#include <internal/rcu.h>

#include <internal/feature.h>
#undef _MTHPC_FEATURE
#define _MTHPC_FEATURE rcu

/* User (global) rcu data */

__thread struct mthpc_rcu_node *mthpc_rcu_node_ptr = NULL;
static struct mthpc_rcu_data mthpc_rcu_data;

/*
 * Maintain all of the rcu data.
 * But let the other feature control (write) their own
 * constructor and destructor of rcu_data in rcu_init/rcu_exit
 */
struct mthpc_rcu_meta {
    struct mthpc_rcu_data *head;
    spinlock_t lock;
};
static struct mthpc_rcu_meta mthpc_rcu_meta;

static void mthpc_wait_for_readers(struct mthpc_rcu_data *data,
                                   unsigned long gp_seq)
{
    struct mthpc_rcu_node *node;
    unsigned long node_gp;

    for (node = data->head; node; node = node->next) {
        while (1) {
            node_gp = atomic_load_explicit(&node->gp_seq, memory_order_consume);
            if (!(node_gp & MTHPC_GP_CTR_NEST_MASK))
                break;
            if (!((node_gp ^ gp_seq) & MTHPC_GP_CTR_PHASE))
                break;
        }
    }
}

void mthpc_synchronize_rcu_internal(struct mthpc_rcu_data *data)
{
    unsigned long curr_gp;

    smp_mb();

    spin_lock(&data->lock);

    curr_gp = atomic_load_explicit(&data->gp_seq, memory_order_relaxed);

    mthpc_wait_for_readers(data, curr_gp);

    smp_mb();
    atomic_store_explicit(
        &data->gp_seq,
        atomic_load_explicit(&data->gp_seq, memory_order_relaxed) ^
            MTHPC_GP_CTR_PHASE,
        memory_order_relaxed);
    smp_mb();

    mthpc_wait_for_readers(data, curr_gp);

    spin_unlock(&data->lock);

    smp_mb();
}

void mthpc_synchronize_rcu(void)
{
    mthpc_synchronize_rcu_internal(&mthpc_rcu_data);
}

void mthpc_synchronize_rcu_all(void)
{
    struct mthpc_rcu_data *data;
    spin_lock(&mthpc_rcu_meta.lock);
    data = mthpc_rcu_meta.head;
    while (data) {
        mthpc_synchronize_rcu_internal(data);
        data = data->next;
    }
    spin_unlock(&mthpc_rcu_meta.lock);
}

/* init/exit function */

void mthpc_rcu_add(struct mthpc_rcu_data *data, unsigned int id,
                   struct mthpc_rcu_node **rev)
{
    struct mthpc_rcu_node **indirect = &data->head;
    struct mthpc_rcu_node *node;

    node = malloc(sizeof(struct mthpc_rcu_node));
    if (!node) {
        MTHPC_WARN_ON(1, "allocation failed");
        return;
    }

    node->id = id;
    atomic_init(&node->gp_seq, 0);
    node->next = NULL;
    node->data = data;

    spin_lock(&data->lock);
    while (*indirect) {
        if ((*indirect)->id == id) {
            spin_unlock(&data->lock);
            free(node);
            MTHPC_WARN_ON(1, "already existed (%6u)", id);
            *rev = *indirect;
            return;
        }
        indirect = &(*indirect)->next;
    }
    *indirect = node;
    spin_unlock(&data->lock);

    *rev = node;
}

void mthpc_rcu_del(struct mthpc_rcu_data *data, unsigned int id,
                   struct mthpc_rcu_node *node)
{
    struct mthpc_rcu_node **indirect = &data->head;
    struct mthpc_rcu_node *target;

    spin_lock(&data->lock);
    while (*indirect) {
        if ((*indirect)->id == id) {
            MTHPC_WARN_ON(node && *indirect != node,
                          "not the same node(%p, %p)", *indirect, node);
            target = *indirect;
            *indirect = (*indirect)->next;
            spin_unlock(&data->lock);
            free(target);
            return;
        }
        indirect = &(*indirect)->next;
    }
    spin_unlock(&data->lock);

    MTHPC_WARN_ON(1, "target node(id=%u) not found", id);
}
void mthpc_rcu_thread_init(void)
{
    if (mthpc_rcu_node_ptr)
        return;
    mthpc_rcu_add(&mthpc_rcu_data, (unsigned long)pthread_self(),
                  &mthpc_rcu_node_ptr);
}

void mthpc_rcu_thread_exit(void)
{
    if (!mthpc_rcu_node_ptr)
        return;
    mthpc_rcu_del(&mthpc_rcu_data, (unsigned long)pthread_self(),
                  mthpc_rcu_node_ptr);
    mthpc_rcu_node_ptr = NULL;
}

void mthpc_rcu_data_init(struct mthpc_rcu_data *data, unsigned int type)
{
    data->head = NULL;
    data->type = type;
    atomic_init(&data->gp_seq, MTHPC_GP_COUNT);
    spin_lock_init(&data->lock);

    spin_lock(&mthpc_rcu_meta.lock);
    data->next = mthpc_rcu_meta.head;
    mthpc_rcu_meta.head = data;
    spin_unlock(&mthpc_rcu_meta.lock);
}

static void mthpc_rcu_data_exit(struct mthpc_rcu_data *data)
{
    struct mthpc_rcu_node *node, *tmp;

    spin_lock(&data->lock);
    node = data->head;
    while (node) {
        tmp = node->next;
        free(node);
        node = tmp;
    }
    data->head = NULL;
    spin_unlock(&data->lock);
    spin_lock_destroy(&data->lock);
}

/* Provide the API to let the other feature can create their own rcu data. */

static void __mthpc_init mthpc_rcu_init(void)
{
    mthpc_init_feature();
    mthpc_rcu_meta.head = NULL;
    spin_lock_init(&mthpc_rcu_meta.lock);
    mthpc_rcu_data_init(&mthpc_rcu_data, MTHPC_RCU_USR);
    mthpc_init_ok();
}

static void __mthpc_exit mthpc_rcu_exit(void)
{
    mthpc_exit_feature();
    mthpc_synchronize_rcu_all();
    mthpc_rcu_data_exit(&mthpc_rcu_data);
    spin_lock_destroy(&mthpc_rcu_meta.lock);
    mthpc_exit_ok();
}
