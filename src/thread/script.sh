#!/usr/bin/env bash

DIR="$(pwd)/../../"
INCLUDE="$DIR/include"
CFLAGS="-g -rdynamic -fsanitize=thread -pthread -I$INCLUDE"
#CFLAGS="-g -rdynamic -pthread -I$INCLUDE"

SRC="test_async.c"

make -C $DIR clean
make -C $DIR debug=1
#clang -o test test.c rcu.c ../centralized_barrier/centralized_barrier.c $CFLAGS
#gcc -S -fverbose-asm test.c $DIR/libmthpc.a $CFLAGS
gcc -o test $SRC $DIR/libmthpc.a $CFLAGS
#TSAN_OPTIONS="history_size=5 verbosity=2 flush_memory_ms=20 force_seq_cst_atomics=1" ./test
#TSAN_OPTIONS="history_size=5 verbosity=2 force_seq_cst_atomics=1" ./test
#TSAN_OPTIONS="force_seq_cst_atomics=1" ./test
./test
#rm -f test
make -C $DIR clean
