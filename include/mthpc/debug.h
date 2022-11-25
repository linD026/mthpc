#ifndef __MTHPC_DEBUG_H__
#define __MTHPC_DEBUG_H__

#include <stdio.h>
#include <execinfo.h>

#include <util.h>

#define mthpc_debug_stream stdout
#define mthpc_err_stream stderr

#define mthpc_print(fmt, ...)                            \
    do {                                                 \
        fprintf(mthpc_debug_stream, fmt, ##__VA_ARGS__); \
    } while (0)

#define mthpc_pr_info(fmt, ...)                                                \
    do {                                                                       \
        mthpc_print("\e[32m%s:%d:%s:\e[0m " fmt, __FILE__, __LINE__, __func__, \
                    ##__VA_ARGS__);                                            \
    } while (0)

#define mthpc_pr_err(fmt, ...)                                \
    do {                                                      \
        fprintf(mthpc_err_stream,                             \
                "\e[32m%s:%d:%s:\e[0m "                       \
                "\e[31m" fmt "\e[0m",                         \
                __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
    } while (0)

#define MTHPC_BUG_ON(cond, fmt, ...)               \
    do {                                           \
        if (mthpc_unlikely(cond)) {                \
            mthpc_pr_err(fmt "\n", ##__VA_ARGS__); \
            exit(EXIT_FAILURE);                    \
        }                                          \
    } while (0)

#define MTHPC_WARN_ON(cond, fmt, ...)              \
    do {                                           \
        if (mthpc_unlikely(cond))                  \
            mthpc_pr_err(fmt "\n", ##__VA_ARGS__); \
    } while (0)

#define MTHPC_STACK_BUF_SIZE 32

mthpc_always_inline void mthpc_dump_stack(void)
{
    char **stack_info;
    int nr = 0;
    void *buf[MTHPC_STACK_BUF_SIZE];

    nr = backtrace(buf, MTHPC_STACK_BUF_SIZE);
    stack_info = backtrace_symbols(buf, nr);

    mthpc_print("========== dump stack start ==========\n");
    for (int i = 0; i < nr; i++)
        mthpc_print("  %s\n", stack_info[i]);
    mthpc_print("========== dump stack  end  ==========\n");
}

#endif /* __MTHPC_DEBUG_H__*/
