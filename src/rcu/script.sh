#!/usr/bin/env bash

DIR="$(pwd)/../../"
INCLUDE="$DIR/include"
CFLAGS="-g -rdynamic -fsanitize=thread -pthread -I$INCLUDE"
#CFLAGS="-g -rdynamic -pthread -I$INCLUDE"

make -C $DIR clean
make -C $DIR
gcc -o test test.c $DIR/libmthpc.a $CFLAGS
TSAN_OPTIONS="history_size=5 flush_memory_ms=20 force_seq_cst_atomics=1" ./test
#TSAN_OPTIONS="force_seq_cst_atomics=1" ./test
#rm -f test
make -C $DIR clean
