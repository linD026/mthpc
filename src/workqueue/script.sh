#!/usr/bin/env bash

DIR="$(pwd)/../../"
INCLUDE="$DIR/include"
#CFLAGS="-g -rdynamic -pthread -I$INCLUDE"
CFLAGS="-g -rdynamic -fsanitize=thread -pthread -I$INCLUDE"

make -C $DIR clean
make -C $DIR debug=1
gcc -o test test.c $DIR/libmthpc.a $CFLAGS
#./test
TSAN_OPTIONS="force_seq_cst_atomics=1" ./test
rm -f test
make -C $DIR clean
