#!/usr/bin/env bash

DIR="$(pwd)/../../"
INCLUDE="$DIR/include"
#CFLAGS="-g -rdynamic -pthread -I$INCLUDE"
CFLAGS="-g -rdynamic -fsanitize=thread -pthread -I$INCLUDE"
SRC="../rcu/rcu.c ../centralized_barrier/centralized_barrier.c workqueue.c"

make -C $DIR clean
make -C $DIR debug=1
clang -o test test.c $SRC $CFLAGS
#gcc -o test test.c $DIR/libmthpc.a $CFLAGS
#./test
TSAN_OPTIONS="verbosity=2 force_seq_cst_atomics=1" ./test
#rm -f test
make -C $DIR clean
