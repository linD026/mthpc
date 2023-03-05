#ifndef __MTHPC_DEBUG_H__
#define __MTHPC_DEBUG_H__

#include <stdlib.h>
#include <execinfo.h>

#include <mthpc/print.h>
#include <mthpc/util.h>

static __always_inline void mthpc_dump_stack(void)
{
#define MTHPC_STACK_BUF_SIZE 32
    char **stack_info;
    int nr = 0;
    void *buf[MTHPC_STACK_BUF_SIZE];

    nr = backtrace(buf, MTHPC_STACK_BUF_SIZE);
    stack_info = backtrace_symbols(buf, nr);

    mthpc_print("========== dump stack start ==========\n");
    for (int i = 0; i < nr; i++)
        mthpc_print("  %s\n", stack_info[i]);
    mthpc_print("========== dump stack  end  ==========\n");
#undef MTHPC_STACK_BUF_SIZE
}

#define MTHPC_BUG_ON(cond, fmt, ...)                                     \
    do {                                                                 \
        if (unlikely(cond)) {                                            \
            mthpc_pr_err("BUG ON: " #cond ", " fmt "\n", ##__VA_ARGS__); \
            mthpc_dump_stack();                                          \
            exit(EXIT_FAILURE);                                          \
        }                                                                \
    } while (0)

#define MTHPC_WARN_ON(cond, fmt, ...)                                    \
    do {                                                                 \
        if (unlikely(cond))                                              \
            mthpc_pr_err("WARN ON:" #cond ", " fmt "\n", ##__VA_ARGS__); \
    } while (0)

#endif /* __MTHPC_DEBUG_H__*/
