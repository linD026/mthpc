#ifndef __MTHPC_DEBUG_H__
#define __MTHPC_DEBUG_H__

#include <stdio.h>
#include <execinfo.h>

#include <util.h>

#define mthpc_debug_stream stderr

#define mthpc_printdg(fmt, ...)                                           \
    do {                                                                  \
        fprintf(mthpc_debug_stream, "%s:%d:%s: " fmt, __FILE__, __LINE__, \
                __func__, ##__VA_ARGS__);                                 \
    } while (0)

#define MTHPC_BUG_ON(cond, fmt, ...)                                 \
    do {                                                             \
        if (mthpc_unlikely(cond)) {                                  \
            mthpc_printdg(fmt "\n", __VA_ARGS__) exit(EXIT_FAILURE); \
            exit(EXIT_FAILURE);                                      \
        }                                                            \
    } while (0)

#define MTHPC_WARN_ON(cond, fmt, ...)            \
    do {                                         \
        if (mthpc_unlikely(cond))                \
            mthpc_printdg(fmt "\n", __VA_ARGS__) \
    } while (0)

#define MTHPC_STACK_BUF_SIZE 32

mthpc_always_inline void mthpc_dump_stack(void)
{
    char **stack_info;
    int nr = 0;
    void *buf[MTHPC_STACK_BUF_SIZE];

    nr = backtrace(buf, MTHPC_STACK_BUF_SIZE);
    stack_info = backtrace_symbols(buf, nr);

    mthpc_printdg("========== dump stack start ==========\n");
    for (int i = 0; i < nr; i++)
        mthpc_printdg("    %s\n", stack_info[i]);
    mthpc_printdg("========== dump stack  end  ==========\n");
}

#endif /* __MTHPC_DEBUG_H__*/
