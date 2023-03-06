#ifndef __MTHPC_PRINT_H__
#define __MTHPC_PRINT_H__

#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>

#define mthpc_debug_stream stdout
#define mthpc_err_stream stderr

static inline struct timeval ___mthpc_get_us(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return tv;
}

#define mthpc_print(fmt, ...)                            \
    do {                                                 \
        fprintf(mthpc_debug_stream, fmt, ##__VA_ARGS__); \
    } while (0)

#define mthpc_pr_info(fmt, ...)                                         \
    do {                                                                \
        struct timeval __pr_tv = ___mthpc_get_us();                     \
        mthpc_print("\e[32m[%5lu.%06lu]\e[0m %s:%d:%s: " fmt,           \
                    (unsigned long)__pr_tv.tv_sec,                      \
                    (unsigned long)__pr_tv.tv_usec, __FILE__, __LINE__, \
                    __func__, ##__VA_ARGS__);                           \
    } while (0)

#define mthpc_pr_err(fmt, ...)                                                 \
    do {                                                                       \
        struct timeval __pr_tv = ___mthpc_get_us();                            \
        fprintf(mthpc_err_stream,                                              \
                "\e[32m[%5lu.%06lu] [TASK %lx]\e[0m %s:%d:%s: "                \
                "\e[31m" fmt "\e[0m",                                          \
                (unsigned long)__pr_tv.tv_sec, (unsigned long)__pr_tv.tv_usec, \
                (unsigned long)pthread_self(), __FILE__, __LINE__, __func__,   \
                ##__VA_ARGS__);                                                \
    } while (0)

#endif /* __MTHPC_PRINT_H__ */
