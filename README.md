# mthpc

mthpc is parallelism and concurrency library for C language. It targets to
improve the parallel and concurrent part of userspace C. mtphc provides the
thread framework, asynchronous worker (deferred work), synchronous mechanisms,
and some concurrent data structures. mthpc is based on [GCC builtin atomic operation](https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html)
and [POSIX Threads](https://en.wikipedia.org/wiki/Pthreads).

## Build

To build the mthpc library, use the following commands.

```bash
make                # Build the library and headers in mthpc directory
make clean          # Delete generated files
```

The default building will generate the dynamic library. If you want to generate
the static library, add the paramter `static=1`.

---

## Features

The mthpc library provides the synchronous mechanisms, concurrncy data
structures, and more. See all the features in [api.md](doc/api.md).

---

## Development

For the development, the mthpc library provides the Makefile paramter,
 `debug=1`, to verify the action in runtime.

All the featues have the test program(s) in their own directory, for example,
rcu feature hase `new_test.c` and `test.c` in `src/rcu/`. And, we can run the
test program(s) with the command, `bash script.sh`.
