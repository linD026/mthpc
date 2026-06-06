#ifndef __PBST_H__
#define __PBST_H__

#include <pthread.h>
#include <stdbool.h>

#ifndef MAX_THREAD_COUNT
#define MAX_THREAD_COUNT 20
#endif /* MAX_THREAD_COUNT */

/* pbst->flags */
#define PBST_ROOT 0x0001 /* NODE is ~PBST_ROOT */
#define PBST_CURR_NULL 0x0002 /* For empty current */
#define PBST_LEFT_NULL 0x0004 /* For empty left */
#define PBST_RIGHT_NULL 0x0008 /* For empty right */
#define PBST_DEAD       0x0010 /* For deleted node */

/* pbst->nodes[] */
#define PBST_NODE_CURR   0
#define PBST_NODE_LEFT   1
#define PBST_NODE_RIGHT  2

#ifdef __cplusplus
extern "C" {
#endif

struct __pbst_node {
    int val;
    /* next */
    struct pbst *parent;
    struct pbst *left;
    struct pbst *right;
};

struct pbst {
    unsigned int flags;
    pthread_rwlock_t lock;
    /* root node */

    /* internal node */
    struct __pbst_node nodes[3]; /* parent, left, right */
};

struct pbst *pbst_init(void);
void pbst_exit(struct pbst *root);

int pbst_is_root(struct pbst *t);
static inline int pbst_is_node(struct pbst *t)
{
    return !pbst_is_root(t);
}

struct pbst *pbst_insert(struct pbst *root, int val);
struct pbst *pbst_delete(struct pbst *root, int val);
bool pbst_lookup(struct pbst *root, int val);

void dump_pbst(struct pbst *t);
void dump_entire_pbst(struct pbst *root);


#ifdef __cplusplus
}
#endif

#endif /* __PBST_H__ */
