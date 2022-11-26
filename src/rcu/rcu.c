#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

#include <mthpc/rcu.h>
#include <mthpc/debug.h>
#include <mthpc/util.h>

#include <internal/rcu.h>

/* User (global) rcu data */

static __thread struct mthpc_rcu_node *mthpc_rcu_node_ptr;
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

void mthpc_rcu_read_lock_internal(struct mthpc_rcu_node *node)
{
    MTHPC_WARN_ON(!node, "rcu node == NULL");

    /* Should be odd number */
    MTHPC_WARN_ON(
        !(__atomic_add_fetch(&node->seqcount, 1, __ATOMIC_RELAXED) & 0x1U),
        "rcu read side lock");
}

void mthpc_rcu_read_unlock_internal(struct mthpc_rcu_node *node)
{
    MTHPC_WARN_ON(!node, "rcu node == NULL");

    MTHPC_WARN_ON(__atomic_add_fetch(&node->seqcount, 1, __ATOMIC_RELAXED) &
                      0x1U,
                  "rcu read side unlock");
}

void mthpc_rcu_read_lock(void)
{
    mthpc_rcu_read_lock_internal(mthpc_rcu_node_ptr);
}

void mthpc_rcu_read_unlock(void)
{
    mthpc_rcu_read_unlock_internal(mthpc_rcu_node_ptr);
}

static mthpc_always_inline bool mthpc_this_gp(struct mthpc_rcu_node *node)
{
    return READ_ONCE(node->seqcount) & 0x1U;
}

void mthpc_synchronize_rcu_internal(struct mthpc_rcu_data *data)
{
    struct mthpc_rcu_node *node;

    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    pthread_mutex_lock(&data->lock);

    for (node = data->head; node; node = node->next) {
        while (mthpc_this_gp(node))
            mthpc_cmb();
    }

    __atomic_thread_fence(__ATOMIC_SEQ_CST);

    /*
     * Some readers may get in to critical section after
     * first time checking. So, check again.
     */
    for (node = data->head; node; node = node->next) {
        while (mthpc_this_gp(node))
            mthpc_cmb();
    }

    pthread_mutex_unlock(&data->lock);

    __atomic_thread_fence(__ATOMIC_SEQ_CST);
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
    node->seqcount = 0;
    node->next = NULL;

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

void mthpc_rcu_thread_init(void)
{
    mthpc_rcu_add(&mthpc_rcu_data, (unsigned int)pthread_self(),
                  &mthpc_rcu_node_ptr);
}

void mthpc_rcu_data_init(struct mthpc_rcu_data *data, unsigned int type)
{
    data->head = NULL;
    data->type = type;
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

static void mthpc_init(mthpc_indep) mthpc_rcu_init(void)
{
    mthpc_rcu_meta.head = NULL;
    pthread_mutex_init(&mthpc_rcu_meta.lock, NULL);
    mthpc_rcu_data_init(&mthpc_rcu_data, MTHPC_RCU_USR);
    mthpc_pr_info("rcu init\n");
}

static void mthpc_exit(mthpc_indep) mthpc_rcu_exit(void)
{
    mthpc_synchronize_rcu_all();
    mthpc_rcu_data_exit(&mthpc_rcu_data);
    pthread_mutex_destroy(&mthpc_rcu_meta.lock);
    mthpc_pr_info("rcu exit\n");
}
