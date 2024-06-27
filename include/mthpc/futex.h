#ifndef __MTHPC_FUTEX_H__
#define __MTHPC_FUTEX_H__

#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <poll.h>

#include <linux/futex.h> /* Definition of FUTEX_* constants */
#include <sys/syscall.h> /* Definition of SYS_* constants */
#include <unistd.h>

#include <mthpc/util.h>

// TODO: support non-linux system
//#if (defined(__linux__) && defined(__NR_futex))

static inline int futex(int32_t *uaddr, int op, int32_t val,
                        const struct timespec *timeout, int32_t *uaddr2,
                        int32_t val3)
{
    return syscall(__NR_futex, uaddr, op, val, timeout, uaddr2, val3);
}

static inline int futex_async(int32_t *uaddr, int op, int32_t val,
                              const struct timespec *timeout, int32_t *uaddr2,
                              int32_t val3)
{
    int ret;

    ret = futex(uaddr, op, val, timeout, uaddr2, val3);
    if (unlikely(ret < 0 && errno == ENOSYS)) {
        ret = 0;

        smp_mb();

        switch (op) {
        case FUTEX_WAIT:
            while (READ_ONCE(*uaddr) == val) {
                if (poll(NULL, 0, 10) < 0) {
                    ret = -1;
                    /* Keep poll errno. Caller handles EINTR. */
                    goto end;
                }
            }
            break;
        case FUTEX_WAKE:
            break;
        default:
            errno = EINVAL;
            ret = -1;
        }
    end:
        return ret;
    }
    return ret;
}
#endif /* __MTHPC_FUTEX_H__ */
