#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>

#define debug_err_stream stderr

#define BUG_ON(cond, fmt, ...)                                         \
    do {                                                               \
        if ((cond)) {                                                  \
            fprintf(debug_err_stream, "%s:%d:%s: " fmt "\n", __FILE__, \
                    __LINE__, __func__, ##__VA_ARGS__);                \
            exit(EXIT_FAILURE);                                        \
        }                                                              \
    } while (0)

#define WARN_ON(cond, fmt, ...)                                        \
    do {                                                               \
        if ((cond)) {                                                  \
            fprintf(debug_err_stream, "%s:%d:%s: " fmt "\n", __FILE__, \
                    __LINE__, __func__, ##__VA_ARGS__);                \
        }                                                              \
    } while (0)

#endif /* __DEBUG_H__ */
