#ifndef __MTHPC_INTERNAL_RCU_H__
#define __MTHPC_INTERNAL_RCU_H__

#include <pthread.h>

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
 * The seqcount will change to protect per object rather
 * than thread from user rcu_data.
 */

struct mthpc_rcu_node {
    unsigned int id;
    unsigned int seqcount;
    struct mthpc_rcu_node *next;
};

struct mthpc_rcu_data {
    pthread_mutex_t lock;
    struct mthpc_rcu_node *head;
    unsigned int type;

    struct mthpc_rcu_data *next;
};

#define MTHPC_RCU_USR 0x0001U
#define MTHPC_RCU_WORKQUEUE 0x0002U
#define MTHPC_RCU_TYPE_MASK (MTHPC_RCU_USER | MTHPC_RCU_WORKQUEUE)

void mthpc_rcu_data_init(struct mthpc_rcu_data *data, unsigned int type);
void mthpc_rcu_add(struct mthpc_rcu_data *data, unsigned int id,
                   struct mthpc_rcu_node **rev);
void mthpc_rcu_read_lock_internal(struct mthpc_rcu_node *node);
void mthpc_rcu_read_unlock_internal(struct mthpc_rcu_node *node);
void mthpc_synchronize_rcu_internal(struct mthpc_rcu_data *data);

void mthpc_synchronize_rcu_all(void);

#endif /* __MTHPC_INTERNAL_RCU_H__ */
