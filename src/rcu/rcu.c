#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

#include <mthpc/rcu.h>
#include <mthpc/debug.h>
#include <mthpc/util.h>

#include <internal/rcu.h>

#include <internal/feature.h>
#undef _MTHPC_FEATURE
#define _MTHPC_FEATURE rcu

/* User (global) rcu data */

__thread struct mthpc_rcu_node *mthpc_rcu_node_ptr;
static struct mthpc_rcu_data mthpc_rcu_data;

/*
 * Maintain all of the rcu data.
 * But let the other feature control (write) their own
 * constructor and destructor of rcu_data in rcu_init/rcu_exit
 */
struct mthpc_rcu_meta {
    struct mthpc_rcu_data *head;
    pthread_mutex_t lock;
};
static struct mthpc_rcu_meta mthpc_rcu_meta;

void mthpc_synchronize_rcu_internal(struct mthpc_rcu_data *data)
{
    struct mthpc_rcu_node *node;
    unsigned long curr_gp;

    smp_mb();

    pthread_mutex_lock(&data->lock);

    curr_gp = data->gp_seq;
    for (node = data->head; node; node = node->next) {
        while (READ_ONCE(node->count) && READ_ONCE(node->count) <= data->gp_seq)
            mthpc_cmb();
    }

    curr_gp = __atomic_fetch_add(&data->gp_seq, 1, __ATOMIC_RELEASE);
    smp_mb();

    for (node = data->head; node; node = node->next) {
        while (READ_ONCE(node->count) && READ_ONCE(node->count) <= curr_gp)
            mthpc_cmb();
    }

    pthread_mutex_unlock(&data->lock);

    smp_mb();
}

void mthpc_synchronize_rcu(void)
{
    mthpc_synchronize_rcu_internal(&mthpc_rcu_data);
}

void mthpc_synchronize_rcu_all(void)
{
    struct mthpc_rcu_data *data;
    pthread_mutex_lock(&mthpc_rcu_meta.lock);
    data = mthpc_rcu_meta.head;
    while (data) {
        mthpc_synchronize_rcu_internal(data);
        data = data->next;
    }
    pthread_mutex_unlock(&mthpc_rcu_meta.lock);
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
    node->count = 0;
    node->next = NULL;
    node->data = data;

    pthread_mutex_lock(&data->lock);
    while (*indirect) {
        if ((*indirect)->id == id) {
            pthread_mutex_unlock(&data->lock);
            free(node);
            MTHPC_WARN_ON(1, "already existed");
            return;
        }
        indirect = &(*indirect)->next;
    }
    *indirect = node;
    pthread_mutex_unlock(&data->lock);

    *rev = node;
}

void mthpc_rcu_del(struct mthpc_rcu_data *data, unsigned int id,
                   struct mthpc_rcu_node *node)
{
    struct mthpc_rcu_node **indirect = &data->head;
    struct mthpc_rcu_node *target;

    pthread_mutex_lock(&data->lock);
    while (*indirect) {
        if ((*indirect)->id == id) {
            MTHPC_WARN_ON(node && *indirect != node,
                          "not the same node(%p, %p)", *indirect, node);
            target = *indirect;
            *indirect = (*indirect)->next;
            pthread_mutex_unlock(&data->lock);
            free(target);
            return;
        }
        indirect = &(*indirect)->next;
    }
    pthread_mutex_unlock(&data->lock);

    MTHPC_WARN_ON(1, "target node(id=%u) not found", id);
}
void mthpc_rcu_thread_init(void)
{
    mthpc_rcu_add(&mthpc_rcu_data, (unsigned int)pthread_self(),
                  &mthpc_rcu_node_ptr);
}

void mthpc_rcu_thread_exit(void)
{
    mthpc_rcu_del(&mthpc_rcu_data, (unsigned int)pthread_self(),
                  mthpc_rcu_node_ptr);
    mthpc_rcu_node_ptr = NULL;
}

void mthpc_rcu_data_init(struct mthpc_rcu_data *data, unsigned int type)
{
    data->head = NULL;
    data->type = type;
    data->gp_seq = 1;
    pthread_mutex_init(&data->lock, NULL);

    pthread_mutex_lock(&mthpc_rcu_meta.lock);
    data->next = mthpc_rcu_meta.head;
    mthpc_rcu_meta.head = data;
    pthread_mutex_unlock(&mthpc_rcu_meta.lock);
}

static void mthpc_rcu_data_exit(struct mthpc_rcu_data *data)
{
    struct mthpc_rcu_node *node, *tmp;

    pthread_mutex_lock(&data->lock);
    node = data->head;
    while (node) {
        tmp = node->next;
        free(node);
        node = tmp;
    }
    pthread_mutex_unlock(&data->lock);
    pthread_mutex_destroy(&data->lock);
}

/* Provide the API to let the other feature can create their own rcu data. */

static void __mthpc_init mthpc_rcu_init(void)
{
    mthpc_init_feature();
    mthpc_rcu_meta.head = NULL;
    pthread_mutex_init(&mthpc_rcu_meta.lock, NULL);
    mthpc_rcu_data_init(&mthpc_rcu_data, MTHPC_RCU_USR);
    mthpc_init_ok();
}

static void __mthpc_exit mthpc_rcu_exit(void)
{
    mthpc_exit_feature();
    mthpc_synchronize_rcu_all();
    mthpc_rcu_data_exit(&mthpc_rcu_data);
    pthread_mutex_destroy(&mthpc_rcu_meta.lock);
    mthpc_exit_ok();
}
