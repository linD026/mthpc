// TODO: Change to use mthpc sync/thread api
/*
 * Fine-grained locking parallel BST:
 * TODO:
 *     - Avoid recursive function call (for locking)
 *     - Performance issue due to locking too many times
 *     - Support balancing
 */
#include <mthpc/ds/pbst.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define DEBUG_PBST(_pbst, fmt, ...)           \
    do {                                      \
        printf("debug: " fmt, ##__VA_ARGS__); \
        dump_pbst(_pbst);                     \
    } while (0)

/* Default is node */
static struct pbst *pbst_alloc(void)
{
    struct pbst *t = (struct pbst *)malloc(sizeof(struct pbst));
    if (!t)
        return NULL;

    t->flags = PBST_CURR_NULL | PBST_LEFT_NULL | PBST_RIGHT_NULL;
    pthread_rwlock_init(&t->lock, NULL);

    t->nodes[PBST_NODE_CURR].val = -1;
    t->nodes[PBST_NODE_CURR].parent = NULL;
    t->nodes[PBST_NODE_CURR].right = t;
    t->nodes[PBST_NODE_CURR].left = t;

    t->nodes[PBST_NODE_LEFT].val = -1;
    t->nodes[PBST_NODE_LEFT].parent = t;
    t->nodes[PBST_NODE_LEFT].right = NULL;
    t->nodes[PBST_NODE_LEFT].left = NULL;

    t->nodes[PBST_NODE_RIGHT].val = -1;
    t->nodes[PBST_NODE_RIGHT].parent = t;
    t->nodes[PBST_NODE_RIGHT].right = NULL;
    t->nodes[PBST_NODE_RIGHT].left = NULL;

    return t;
}

static void pbst_free(struct pbst *t)
{
    pthread_rwlock_destroy(&t->lock);
    free(t);
}

int pbst_is_root(struct pbst *t)
{
    return !!(t->flags & PBST_ROOT);
}

struct pbst *pbst_init(void)
{
    struct pbst *t = pbst_alloc();

    t->flags |= PBST_ROOT;

    return t;
}

void pbst_exit(struct pbst *root)
{
    // TODO:
}

/*
 * TODO: return the node not pbst block
 *
 *
 * When we insert the node, we acquire two locks.
 *
 *                      curr
 *                     /    \
 *                  left   right
 *                 /    \
 *               <a>
 *             /     \
 *          <b>      <c>
 *
 * For example, if we insert the val into <b>, we need to 
 *
 *
 *
 */
struct pbst *pbst_insert(struct pbst *root, int val)
{
    struct pbst *curr = root;
    struct pbst *prealloc = NULL;
    struct pbst *ret = NULL;

    /* prealloc the new space. */
    prealloc = pbst_alloc();
    if (!prealloc) {
        printf("No memory.\n");
        assert(0);
    }
    prealloc->nodes[PBST_NODE_CURR].val = val;

    pthread_rwlock_wrlock(&curr->lock);
    do {
        /* we already hold the lock. */

        /* Check the current node is empty */
        if (curr->flags & PBST_CURR_NULL) {
            curr->nodes[PBST_NODE_CURR].val = val;
            curr->flags &= ~PBST_CURR_NULL;
            ret = curr;
            pthread_rwlock_unlock(&curr->lock);
            /* Updated the current node, bailout */
            break;
        }

        /* current node is non-empty, check child. */
        if (curr->nodes[PBST_NODE_CURR].val > val) {
            /* left child */
            /* Check the left child node is empty */
            if (curr->flags & PBST_LEFT_NULL) {
                curr->nodes[PBST_NODE_LEFT].val = val;
                curr->flags &= ~PBST_LEFT_NULL;
                ret = curr;
                pthread_rwlock_unlock(&curr->lock);
                /* Updated the left node, bailout */
                break;
            }
            /* left child is non-empty, check the left's child. */
            struct __pbst_node *left = &curr->nodes[PBST_NODE_LEFT];

            /*
             *         curr
             *        /    \
             *      left  right
             *     /    \
             *    <a>   <b>
             */
            if (left->val > val) {
                /* Case <a> */
                if (!left->left) {
                    prealloc->flags &= ~PBST_CURR_NULL;
                    prealloc->nodes[PBST_NODE_CURR].val = val;
                    prealloc->nodes[PBST_NODE_CURR].parent = curr;
                    ret = prealloc;
                    left->left = prealloc;
                    prealloc = NULL;
                    pthread_rwlock_unlock(&curr->lock);
                    break;
                }
                /* pass to next <a> */
                struct pbst *a = left->left;

                // TODO: deadlock?
                /*
                 * Ensure that a is not freed.
                 * NOTE: We need to make sure the deletion is
                 * also topdown locking.
                 */
                pthread_rwlock_wrlock(&a->lock);
                curr = a;
                pthread_rwlock_unlock(
                    &curr->nodes[PBST_NODE_CURR].parent->lock);
                continue;
            }

            /* Case <b> */
            if (!left->right) {
                prealloc->flags &= ~PBST_CURR_NULL;
                prealloc->nodes[PBST_NODE_CURR].val = val;
                prealloc->nodes[PBST_NODE_CURR].parent = curr;
                ret = prealloc;
                left->right = prealloc;
                prealloc = NULL;
                pthread_rwlock_unlock(&curr->lock);
                break;
            }
            /* pass to next <b> */
            struct pbst *b = left->right;

            // TODO: deadlock?
            /*
             * Ensure that <b> is not freed.
             * NOTE: We need to make sure the deletion is
             * also topdown locking.
             */
            pthread_rwlock_wrlock(&b->lock);
            curr = b;
            pthread_rwlock_unlock(&curr->nodes[PBST_NODE_CURR].parent->lock);
            continue;
        } else {
            /* right child */

            /* Check the right child node is empty */
            if (curr->flags & PBST_RIGHT_NULL) {
                curr->nodes[PBST_NODE_RIGHT].val = val;
                curr->flags &= ~PBST_RIGHT_NULL;
                ret = curr;
                pthread_rwlock_unlock(&curr->lock);
                /* Updated the right node, bailout */
                break;
            }
            /* right child is non-empty, check the right's child. */
            struct __pbst_node *right = &curr->nodes[PBST_NODE_RIGHT];

            /*
             *         curr
             *        /    \
             *      left  right
             *           /    \
             *          <a>   <b>
             */
            if (right->val > val) {
                /* Case <a> */
                if (!right->left) {
                    prealloc->flags &= ~PBST_CURR_NULL;
                    prealloc->nodes[PBST_NODE_CURR].val = val;
                    prealloc->nodes[PBST_NODE_CURR].parent = curr;
                    ret = prealloc;
                    right->left = prealloc;
                    prealloc = NULL;
                    pthread_rwlock_unlock(&curr->lock);
                    break;
                }
                /* pass to next <a> */
                struct pbst *a = right->left;

                // TODO: deadlock?
                /*
                 * Ensure that a is not freed.
                 * NOTE: We need to make sure the deletion is
                 * also topdown locking.
                 */
                pthread_rwlock_wrlock(&a->lock);
                curr = a;
                pthread_rwlock_unlock(
                    &curr->nodes[PBST_NODE_CURR].parent->lock);
                continue;
            }

            /* Case <b> */
            if (!right->right) {
                prealloc->flags &= ~PBST_CURR_NULL;
                prealloc->nodes[PBST_NODE_CURR].val = val;
                prealloc->nodes[PBST_NODE_CURR].parent = curr;
                ret = prealloc;
                right->right = prealloc;
                prealloc = NULL;
                pthread_rwlock_unlock(&curr->lock);
                break;
            }
            /* pass to next <b> */
            struct pbst *b = right->right;

            // TODO: deadlock?
            /*
             * Ensure that <b> is not freed.
             * NOTE: We need to make sure the deletion is
             * also topdown locking.
             */
            pthread_rwlock_wrlock(&b->lock);
            curr = b;
            pthread_rwlock_unlock(&curr->nodes[PBST_NODE_CURR].parent->lock);
            continue;
        }
    } while (1);

    if (prealloc)
        pbst_free(prealloc);

    return ret;
}

/*
 * If we get the subtree's root lock, then we hold the entire subtree's wlock.
 */
static int get_and_remove_largest(struct pbst *curr, int *should_cleanup)
{
    int val = -2;

    do {
        pthread_rwlock_wrlock(&curr->lock);
        /*
         * we should always have a node with at least one value (in curr).
         * 
         * Note that in pbst_delete() we already remove the value that we
         * want to, therefore it might has val == -1 with !CURR_NULL.
         */
        if (curr->flags & PBST_CURR_NULL) {
            dump_pbst(curr);
            assert(0);
        }

        /* curr->val != -1, we can assume that this curr will not be freed. */

        /*
         *          \
    	 *         curr
    	 *        /    \
    	 *              right
         *              /   \
         *            <a>    <b>
         * 
         * Check the right child is empty or not
    	 */
        if (!(curr->flags & PBST_RIGHT_NULL)) {
            /* right child is not empty, let's get the largest value here. */
            struct __pbst_node *right = &curr->nodes[PBST_NODE_RIGHT];

            /* if <b> is empty, use the right's value */
            if (!right->right) {
                val = right->val;
                right->val = -1;

                /*
                 * Used right's value, we need to check whether
                 * it has <a> or not.
                 */
                if (right->left) {
                    int largest =
                        get_and_remove_largest(right->left, should_cleanup);
                    // TODO: what if we can not get the largest?
                    right->val = largest;
                    if (*should_cleanup) {
                        right->left = NULL;
                        *should_cleanup = 0;
                    }
                } else {
                    /* we don't have <a> and <b>, let's cleanup the right. */
                    curr->flags |= PBST_RIGHT_NULL;
                }

                pthread_rwlock_unlock(&curr->lock);
                break;
            }
            /* otherwise, <b> is non empty, we go down. */
            val = get_and_remove_largest(right->right, should_cleanup);
            if (*should_cleanup) {
                right->right = NULL;
                *should_cleanup = 0;
            }

            pthread_rwlock_unlock(&curr->lock);
            break;
        }

        /*
         *           \
         *           curr
         *          /    \
         *        left   NULL
         *       /    \
         *     <a>    <b>
         *
         * We don't have right subtree,
         */

        /*
         * right child is empty, let's use curr->val.
         * Make sure we cleanup the memory.
         */
        val = curr->nodes[PBST_NODE_CURR].val;

        /*
         * If left is not empty, we need to find the value for curr.
         */
        if (!(curr->flags & PBST_LEFT_NULL)) {
            /*
             * Ok, the left is non empty.
             */
            struct __pbst_node *left = &curr->nodes[PBST_NODE_LEFT];

            /* if <b> is empty, use the left's value */
            if (!left->right) {
                curr->nodes[PBST_NODE_CURR].val = left->val;
                left->val = -1;

                /*
                 * Used left's value, we need to check whether
                 * it has <a> or not.
                 */
                if (left->left) {
                    int largest =
                        get_and_remove_largest(left->left, should_cleanup);
                    // TODO: what if we can not get the largest?
                    left->val = largest;
                    if (*should_cleanup) {
                        left->left = NULL;
                        *should_cleanup = 0;
                    }
                } else {
                    /* we don't have <a> and <b>, let's cleanup the right. */
                    curr->flags |= PBST_LEFT_NULL;
                }

                pthread_rwlock_unlock(&curr->lock);
                break;
            }
            /* otherwise, <b> is non empty, we go down. */
            curr->nodes[PBST_NODE_CURR].val =
                get_and_remove_largest(left->right, should_cleanup);
            if (*should_cleanup) {
                left->right = NULL;
                *should_cleanup = 0;
            }

            pthread_rwlock_unlock(&curr->lock);
            break;
        }

        /* make this node is unavailable. */
        curr->nodes[PBST_NODE_CURR].val = -1;
        curr->flags = PBST_DEAD;

        pthread_rwlock_unlock(&curr->lock);

        pbst_free(curr);
        /* upper node's pointer point to this node should be cleanup. */
        *should_cleanup = 1;

        break;
    } while (1);

    /*
     * If the stack is empty, tell the upper node that they
     * should cleanup the pointer.
     */
    //if (__deletion_top == 0)
    //    *should_cleanup = 1;

    // release all the hold locks

    return val;
}

/*
 * If we get the subtree's root lock, then we hold the entire subtree's wlock.
 */
static int get_and_remove_smallest(struct pbst *curr, int *should_cleanup)
{
    int val = -2;

    do {
        pthread_rwlock_wrlock(&curr->lock);
        /*
         * we should always have a node with at least one value (in curr).
         * 
         * Note that in pbst_delete() we already remove the value that we
         * want to, therefore it might has val == -1 with !CURR_NULL.
         */
        if (curr->flags & PBST_CURR_NULL) {
            dump_pbst(curr);
            assert(0);
        }

        /* curr->val != -1, we can assume that this curr will not be freed. */

        /*
         *          \
    	 *         curr
    	 *        /    \
    	 *      left
         *      /   \
         *   <a>    <b>
         * 
         * Check the left child is empty or not
    	 */
        if (!(curr->flags & PBST_LEFT_NULL)) {
            /* left is not empty, let's get the smallest value here. */
            struct __pbst_node *left = &curr->nodes[PBST_NODE_LEFT];

            /* if <a> is empty, use the left's value */
            if (!left->left) {
                val = left->val;
                left->val = -1;

                /*
                 * Used left's value, we need to check whether
                 * it has <b> or not.
                 */
                if (left->left) {
                    int smallest =
                        get_and_remove_smallest(left->left, should_cleanup);
                    // TODO: what if we can not get the smallest?
                    left->val = smallest;
                    if (*should_cleanup) {
                        left->left = NULL;
                        *should_cleanup = 0;
                    }
                } else {
                    /* we don't have <a> and <b>, let's cleanup the left. */
                    curr->flags |= PBST_LEFT_NULL;
                }

                pthread_rwlock_unlock(&curr->lock);
                break;
            }
            /* otherwise, <a> is non empty, we go down. */
            val = get_and_remove_smallest(left->left, should_cleanup);
            if (*should_cleanup) {
                left->left = NULL;
                *should_cleanup = 0;
            }

            pthread_rwlock_unlock(&curr->lock);
            break;
        }

        /*
         *           \
         *           curr
         *          /    \
         *        NULL  right
         *              /    \
         *            <a>    <b>
         *
         * We don't have left subtree,
         */

        /*
         * left child is empty, let's use curr->val.
         * Make sure we cleanup the memory.
         */
        val = curr->nodes[PBST_NODE_CURR].val;

        /*
         * If right is not empty, we need to find the value for curr.
         */
        if (!(curr->flags & PBST_RIGHT_NULL)) {
            /*
             * Ok, the right is non empty.
             */
            struct __pbst_node *right = &curr->nodes[PBST_NODE_RIGHT];

            /* if <a> is empty, use the right's value */
            if (!right->left) {
                curr->nodes[PBST_NODE_CURR].val = right->val;
                /* currently, we have right->val == -1 && !RIGHT_NULL */
                right->val = -1;

                /*
                 * Used right's value, we need to check whether
                 * it has <b> or not.
                 */
                if (right->right) {
                    int smallest =
                        get_and_remove_smallest(right->right, should_cleanup);
                    // TODO: what if we can not get the smallest?
                    if (smallest == -1) {
                        dump_pbst(right->right);
                        assert(0);
                    }
                    right->val = smallest;
                    if (*should_cleanup) {
                        right->right = NULL;
                        *should_cleanup = 0;
                    }
                } else {
                    /* we don't have <a> and <b>, let's cleanup the right. */
                    curr->flags |= PBST_RIGHT_NULL;
                }

                pthread_rwlock_unlock(&curr->lock);
                break;
            }
            /* otherwise, <a> is non empty, we go down. */
            curr->nodes[PBST_NODE_CURR].val =
                get_and_remove_smallest(right->left, should_cleanup);
            if (*should_cleanup) {
                right->left = NULL;
                *should_cleanup = 0;
            }

            pthread_rwlock_unlock(&curr->lock);
            break;
        }

        /* Ok, curr, left, right is empty, we need to free the node. */

        /* make this node is unavailable. */
        curr->nodes[PBST_NODE_CURR].val = -1;
        curr->flags = PBST_DEAD;

        pthread_rwlock_unlock(&curr->lock);

        pbst_free(curr);
        /* upper node's pointer point to this node should be cleanup. */
        *should_cleanup = 1;

        break;
    } while (1);

    /*
     * If the stack is empty, tell the upper node that they
     * should cleanup the pointer.
     */
    //if (__deletion_top == 0)
    //    *should_cleanup = 1;

    // release all the hold locks

    return val;
}

// return 1 if check and cleanup
static int pbst_delete_check_next(struct __pbst_node *parent, struct pbst *next,
                                  int val)
{
    /* we hold parent and next's lock here. */

    if (next->nodes[PBST_NODE_CURR].val == val) {
        if (next->flags & PBST_LEFT_NULL && next->flags & PBST_RIGHT_NULL) {
            next->nodes[PBST_NODE_CURR].val = -1;
            next->flags = PBST_DEAD;

            if (parent->left == next)
                parent->left = NULL;
            else
                parent->right = NULL;

            return 1;
        }
    }

    return 0;
}

/*
 * NOTE: make sure we lock the node from top to down.
 * For example, when we currently at curr and we want to delete <a>
 * We need to get the locks with this order:
 *
 * 		curr->lock
 * 			<a> or <b> -> lock
 *				delete <a> or <b>
 *
 *
 * Remove <a> with only one child:
 *         curr
 *        /    \
 *      left  right
 *           /    \
 *          <a>   <b>
 *         /  \
 *       <c>
 *
 *
 * What if we want to remove <a>?
 *         curr
 *        /    \
 *      left  right
 *           /    \
 *          <a>   <b>
 *         /  \
 *       <c>  <d>
 */
struct pbst *pbst_delete(struct pbst *root, int val)
{
    struct pbst *ret = NULL;
    struct pbst *curr = root;

    pthread_rwlock_wrlock(&curr->lock);
    do {
        /* already hold the lock. */

        /* If current is empty, we do nothing */
        if (curr->flags & PBST_CURR_NULL) {
            pthread_rwlock_unlock(&curr->lock);
            break;
        }

        if (curr->nodes[PBST_NODE_CURR].val == -1) {
            dump_pbst(curr);
            dump_entire_pbst(root);
            fflush(stdout);
            assert(0);
        }

        /* current node is non-empty, check the value */

        /*
         * Case 1:
         * 			curr
         * 		   /    \
         * 		left    right
         *
         * we found @val is curr->val
         */
        if (curr->nodes[PBST_NODE_CURR].val == val) {
            /* we remove the current node's value */
            curr->nodes[PBST_NODE_CURR].val = -1;
            ret = curr;

            /*
             * We always find the left's largest value or
             * right's smallest value to replace it.
             */
            if (!(curr->flags & PBST_LEFT_NULL)) {
                /* left is not empty, let's get the largest value here. */
                struct __pbst_node *left = &curr->nodes[PBST_NODE_LEFT];

                /*
                 * 			curr
                 * 		   /    \
                 * 		left    right
                 *     /    \
                 *   <a>    <b>
                 *
                 * If <b> is empty, left is the largest one.
                 */
                if (!left->right) {
                    /* the largest value is left */
                    curr->nodes[PBST_NODE_CURR].val = left->val;
                    left->val = -1;

                    /*
                     * Used left value, we need to check whether
                     * it has <a> or not.
                     */
                    if (left->left) {
                        int cleanup = 0;
                        int largest =
                            get_and_remove_largest(left->left, &cleanup);
                        left->val = largest;
                        if (cleanup)
                            left->left = NULL;
                    } else {
                        /* we don't have <a> and <b>, let's cleanup the left. */
                        curr->flags |= PBST_LEFT_NULL;
                    }
                    pthread_rwlock_unlock(&curr->lock);
                    break;
                }

                /*
                 * otherwise, <b> is non empty, we go down.
                 */
                int cleanup = 0;
                int largest = get_and_remove_largest(left->right, &cleanup);
                curr->nodes[PBST_NODE_CURR].val = largest;
                if (cleanup)
                    left->right = NULL;
                pthread_rwlock_unlock(&curr->lock);
                break;
            }

            /* We already handled the no childern case. */

            /*
             *
             *        curr
             *       /    \
             *     NULL   right
             *           /     \
             *         <a>      <b>
             *
             * We don't have left subtree.
             * Let's get the smallest value here.
             */
            struct __pbst_node *right = &curr->nodes[PBST_NODE_RIGHT];

            /*
             * If <a> is empty, right is smallest one.
             */
            if (!right->left) {
                /* the smallest value is right */
                curr->nodes[PBST_NODE_CURR].val = right->val;
                /* currently, we have right->val == -1 && !RIGHT_NULL */
                right->val = -1;

                /*
                 * Used right value, we need to check whether
                 * it has <b> or not.
                 */
                if (right->right) {
                    int cleanup = 0;
                    int smallest =
                        get_and_remove_smallest(right->right, &cleanup);
                    right->val = smallest;
                    if (cleanup)
                        right->right = NULL;
                } else {
                    /* we don't have <a> and <b>, let's cleanup the right. */
                    curr->flags |= PBST_RIGHT_NULL;
                }
                pthread_rwlock_unlock(&curr->lock);
                break;
            }

            /*
             * otherwise, <a> is non empty, we go down.
             */
            int cleanup = 0;
            int smallest = get_and_remove_smallest(right->left, &cleanup);
            curr->nodes[PBST_NODE_CURR].val = smallest;
            if (cleanup)
                right->left = NULL;
            pthread_rwlock_unlock(&curr->lock);
            break;
        }

        /* current node is non-empty, and the curr->val is not @val  */

        /* @val is smaller than curr->val */
        if (val < curr->nodes[PBST_NODE_CURR].val) {
            /*
			 * go right subtree:
			 *
			 *        curr
			 *       /    \
			 *    left
             *   /    \
             * <a>     <b>
			 */

            /*
			 * if left child is empty, we have done. bailout.
			 */
            if (curr->flags & PBST_LEFT_NULL) {
                /* Not found. */
                pthread_rwlock_unlock(&curr->lock);
                break;
            }

            /* left child is non empty. */
            struct __pbst_node *left = &curr->nodes[PBST_NODE_LEFT];

            /* Check the left->val is @val or not. */
            if (left->val == val) {
                /*we remove the left's value */
                left->val = -1;
                ret = curr;

                /*
                 * We always find the <a>'s largest value or
                 * <b>'s smallest value to replace it.
                 */
                if (left->left) {
                    /* <a> is not empty, let's get the largest value here */
                    int cleanup = 0;
                    int largest = get_and_remove_largest(left->left, &cleanup);
                    left->val = largest;
                    if (cleanup)
                        left->left = NULL;
                } else if (left->right) {
                    /*
                     * <a> is empty, and <b> is not empty,
                     * let's get the smallest value here.
                     */
                    int cleanup = 0;
                    int smallest =
                        get_and_remove_smallest(left->right, &cleanup);
                    left->val = smallest;
                    if (cleanup)
                        left->right = NULL;
                } else {
                    /* <a> and <b> is empty. */
                    curr->flags |= PBST_LEFT_NULL;
                }

                pthread_rwlock_unlock(&curr->lock);
                break;
            }

            /* @left is not left->val, we need to go down */
            if (val < left->val) {
                /* go to <a> subtree */
                if (left->left) {
                    curr = left->left;

                    pthread_rwlock_wrlock(&curr->lock);

                    if (pbst_delete_check_next(left, curr, val)) {
                        pthread_rwlock_unlock(&curr->lock);
                        pthread_rwlock_unlock(
                            &curr->nodes[PBST_NODE_CURR].parent->lock);
                        pbst_free(curr);
                        break;
                    }

                    pthread_rwlock_unlock(
                        &curr->nodes[PBST_NODE_CURR].parent->lock);
                    continue;
                }
                /* Not found. */
                pthread_rwlock_unlock(&curr->lock);
                break;
            }
            if (left->val < val) {
                /* go to <b> subtree */
                if (left->right) {
                    curr = left->right;

                    pthread_rwlock_wrlock(&curr->lock);

                    if (pbst_delete_check_next(left, curr, val)) {
                        pthread_rwlock_unlock(&curr->lock);
                        pthread_rwlock_unlock(
                            &curr->nodes[PBST_NODE_CURR].parent->lock);
                        pbst_free(curr);
                        break;
                    }

                    pthread_rwlock_unlock(
                        &curr->nodes[PBST_NODE_CURR].parent->lock);
                    continue;
                }
                /* Not found. */
                pthread_rwlock_unlock(&curr->lock);
                break;
            }
            /* Not found. */
            pthread_rwlock_unlock(&curr->lock);
            break;
        }

        /* @val is larger than curr->val */
        if (curr->nodes[PBST_NODE_CURR].val < val) {
            /*
			 * go left subtree:
			 *
			 *        curr
			 *       /    \
			 *           right
             *           /    \
             *         <a>     <b>
			 */

            /*
			 * if right child is empty, we have done. bailout.
			 */
            if (curr->flags & PBST_RIGHT_NULL) {
                /* Not found. */
                pthread_rwlock_unlock(&curr->lock);
                break;
            }

            /* right child is non empty. */
            struct __pbst_node *right = &curr->nodes[PBST_NODE_RIGHT];

            /* Check the right->val is @val or not. */
            if (right->val == val) {
                /*we remove the left's value */
                right->val = -1;
                ret = curr;

                /*
                 * We always find the <a>'s largest value or
                 * <b>'s smallest value to replace it.
                 */
                if (right->left) {
                    /* <a> is not empty, let's get the largest value here */
                    int cleanup = 0;
                    int largest = get_and_remove_largest(right->left, &cleanup);
                    right->val = largest;
                    if (cleanup)
                        right->left = NULL;
                } else if (right->right) {
                    /*
                     * <a> is empty, and <b> is not empty,
                     * let's get the smallest value here.
                     */
                    int cleanup = 0;
                    int smallest =
                        get_and_remove_smallest(right->right, &cleanup);
                    right->val = smallest;
                    if (cleanup)
                        right->right = NULL;
                } else {
                    /* <a> and <b> is empty. */
                    curr->flags |= PBST_RIGHT_NULL;
                }

                pthread_rwlock_unlock(&curr->lock);
                break;
            }

            /* @left is not right->val, we need to go down */
            if (val < right->val) {
                /* go to <a> subtree */
                if (right->left) {
                    curr = right->left;

                    pthread_rwlock_wrlock(&curr->lock);

                    if (pbst_delete_check_next(right, curr, val)) {
                        pthread_rwlock_unlock(&curr->lock);
                        pthread_rwlock_unlock(
                            &curr->nodes[PBST_NODE_CURR].parent->lock);
                        pbst_free(curr);
                        break;
                    }

                    pthread_rwlock_unlock(
                        &curr->nodes[PBST_NODE_CURR].parent->lock);
                    continue;
                }
                /* Not found. */
                pthread_rwlock_unlock(&curr->lock);
                break;
            }
            if (right->val < val) {
                /* go to <b> subtree */
                if (right->right) {
                    curr = right->right;
                    pthread_rwlock_wrlock(&curr->lock);

                    if (pbst_delete_check_next(right, curr, val)) {
                        pthread_rwlock_unlock(&curr->lock);
                        pthread_rwlock_unlock(
                            &curr->nodes[PBST_NODE_CURR].parent->lock);
                        pbst_free(curr);
                        break;
                    }

                    pthread_rwlock_unlock(
                        &curr->nodes[PBST_NODE_CURR].parent->lock);
                    continue;
                }
                /* Not found. */
                pthread_rwlock_unlock(&curr->lock);
                break;
            }
            /* Not found. */
            pthread_rwlock_unlock(&curr->lock);
            break;
        }

    } while (1);

    return ret;
}

bool pbst_lookup(struct pbst *root, int val)
{
    bool ret = false;
    struct pbst *curr = root;

    pthread_rwlock_rdlock(&curr->lock);

    do {
        /* Check the current node is empty */
        if (curr->flags & PBST_CURR_NULL) {
            ret = false;
            pthread_rwlock_unlock(&curr->lock);
            break;
        }

        /*
         * current node is non empty. let's check the value.
         *
         *      curr
         *     /   \
         *   left   right
         *
         */
        if (val == curr->nodes[PBST_NODE_CURR].val) {
            ret = true;
            pthread_rwlock_unlock(&curr->lock);
            break;
        }

        /* curr->val != val, check child. */

        /* check left */
        if (val < curr->nodes[PBST_NODE_CURR].val) {
            if (!(curr->flags & PBST_LEFT_NULL)) {
                struct __pbst_node *left = &curr->nodes[PBST_NODE_LEFT];

                if (left->val == val) {
                    ret = true;
                    pthread_rwlock_unlock(&curr->lock);
                    break;
                }

                if (val < left->val) {
                    if (left->left) {
                        curr = left->left;
                        pthread_rwlock_rdlock(&curr->lock);
                        pthread_rwlock_unlock(
                            &curr->nodes[PBST_NODE_CURR].parent->lock);
                        continue;
                    }
                    /* Not found. */
                    pthread_rwlock_unlock(&curr->lock);
                    break;
                }

                /* left->val < val */

                if (left->right) {
                    curr = left->right;
                    pthread_rwlock_rdlock(&curr->lock);
                    pthread_rwlock_unlock(
                        &curr->nodes[PBST_NODE_CURR].parent->lock);
                    continue;
                }
            }
            /* Not found. */
            pthread_rwlock_unlock(&curr->lock);
            break;
        }

        /* check right */
        if (curr->nodes[PBST_NODE_CURR].val < val) {
            if (!(curr->flags & PBST_RIGHT_NULL)) {
                struct __pbst_node *right = &curr->nodes[PBST_NODE_RIGHT];

                if (right->val == val) {
                    ret = true;
                    pthread_rwlock_unlock(&curr->lock);
                    break;
                }

                if (val < right->val) {
                    if (right->left) {
                        curr = right->left;
                        pthread_rwlock_rdlock(&curr->lock);
                        pthread_rwlock_unlock(
                            &curr->nodes[PBST_NODE_CURR].parent->lock);
                        continue;
                    }
                    /* Not found. */
                    pthread_rwlock_unlock(&curr->lock);
                    break;
                }

                /* left->val < val */

                if (right->right) {
                    curr = right->right;
                    pthread_rwlock_rdlock(&curr->lock);
                    pthread_rwlock_unlock(
                        &curr->nodes[PBST_NODE_CURR].parent->lock);
                    continue;
                }
            }
            /* Not Found. */
            pthread_rwlock_unlock(&curr->lock);
            break;
        }
        pthread_rwlock_unlock(&curr->lock);
    } while (1);

    return ret;
}

/* Debug */

void dump_pbst(struct pbst *t)
{
    if (!t) {
        printf("empty node\n");
        return;
    }

    printf("struct pbst @%p {\n", t);
    printf("    flags: ");
#define __PRINT_FLAGS(__t, __flag) \
    printf("%s", __t->flags &__flag ? " | " #__flag : "")

    __PRINT_FLAGS(t, PBST_ROOT);
    __PRINT_FLAGS(t, PBST_CURR_NULL);
    __PRINT_FLAGS(t, PBST_LEFT_NULL);
    __PRINT_FLAGS(t, PBST_RIGHT_NULL);
    __PRINT_FLAGS(t, PBST_DEAD);
    printf("\n");

    printf("    nodes\n");
    printf("        curr->val: %d\n", t->nodes[PBST_NODE_CURR].val);
    printf("            curr->parent: %p\n", t->nodes[PBST_NODE_CURR].parent);
    printf("            curr->left: %p\n", t->nodes[PBST_NODE_CURR].left);
    printf("            curr->right: %p\n", t->nodes[PBST_NODE_CURR].right);
    printf("        left->val: %d\n", t->nodes[PBST_NODE_LEFT].val);
    printf("            left->parent: %p\n", t->nodes[PBST_NODE_LEFT].parent);
    printf("            left->left: %p\n", t->nodes[PBST_NODE_LEFT].left);
    printf("            left->right: %p\n", t->nodes[PBST_NODE_LEFT].right);
    printf("        right->val: %d\n", t->nodes[PBST_NODE_RIGHT].val);
    printf("            right->parent: %p\n", t->nodes[PBST_NODE_RIGHT].parent);
    printf("            right->left: %p\n", t->nodes[PBST_NODE_RIGHT].left);
    printf("            right->right: %p\n", t->nodes[PBST_NODE_RIGHT].right);
    printf("}\n");
}

static inline void print_preorder(struct pbst *node)
{
    if (!node)
        return;

    dump_pbst(node);

    print_preorder(node->nodes[PBST_NODE_LEFT].left);
    print_preorder(node->nodes[PBST_NODE_LEFT].right);

    print_preorder(node->nodes[PBST_NODE_RIGHT].left);
    print_preorder(node->nodes[PBST_NODE_RIGHT].right);
}

void dump_entire_pbst(struct pbst *root)
{
    printf("\ndump entire pBST:\n");
    print_preorder(root);
    fflush(stdout);
}
