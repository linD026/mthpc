#if !(defined(__linux__) && defined(__NR_futex))

#include <mthpc/futex.h>
#include <mthpc/util.h>
#include <mthpc/debug.h>
#include <pthread.h>

static pthread_mutex_t mthpc_futex_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t mthpc_futex_cond = PTHREAD_COND_INITIALIZER;

int futex(int32_t *uaddr, int op, int32_t val,
                        const struct timespec *timeout, int32_t *uaddr2,
                        int32_t val3)
{
    int lret;
    int ret = 0;

    MTHPC_BUG_ON(timeout, "futex timeout");
    MTHPC_BUG_ON(uaddr2, "futex uaddr2");
    MTHPC_BUG_ON(val3, "futex val3");

    lret = pthread_mutex_lock(&mthpc_futex_mutex);
    if (lret)
        return -EAGAIN;

    switch (op) {
    case FUTEX_WAIT:
        while (READ_ONCE(*uaddr) == val)
            pthread_cond_wait(&mthpc_futex_cond, &mthpc_futex_mutex);
        break;
    case FUTEX_WAKE:
        pthread_cond_broadcast(&mthpc_futex_cond);
        break;
    default:
        ret = -EINVAL;
    }

    lret = pthread_mutex_unlock(&mthpc_futex_mutex);
    if (lret)
        return -EAGAIN;

    return ret;
}
#endif /* !(defined(__linux__) && defined(__NR_futex)) */
