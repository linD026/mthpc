#ifndef __MTHPC_PRINT_H__
#define __MTHPC_PRINT_H__

#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>

#define mthpc_debug_stream stdout
#define mthpc_err_stream stderr

static inline unsigned long __mthpc_get_ms(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return (unsigned long)1000000 * tv.tv_sec + tv.tv_usec;
}

#define mthpc_print(fmt, ...)                            \
    do {                                                 \
        fprintf(mthpc_debug_stream, fmt, ##__VA_ARGS__); \
    } while (0)

#define mthpc_pr_info(fmt, ...)                                             \
    do {                                                                    \
        mthpc_print("\e[32m[%-10lu]\e[0m %s:%d:%s: " fmt, __mthpc_get_ms(), \
                    __FILE__, __LINE__, __func__, ##__VA_ARGS__);           \
    } while (0)

#define mthpc_pr_err(fmt, ...)                                             \
    do {                                                                   \
        fprintf(mthpc_err_stream,                                          \
                "\e[32m[%-10lu] [TASK %lx]\e[0m %s:%d:%s: "                \
                "\e[31m" fmt "\e[0m",                                      \
                __mthpc_get_ms(), (unsigned long)pthread_self(), __FILE__, \
                __LINE__, __func__, ##__VA_ARGS__);                        \
    } while (0)

#endif /* __MTHPC_PRINT_H__ */
