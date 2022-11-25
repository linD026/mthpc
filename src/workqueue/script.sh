#!/usr/bin/env bash

DIR="$(pwd)/../../"
INCLUDE="$DIR/include"
CFLAGS="-g -rdynamic -pthread -I$INCLUDE"

make -C $DIR
gcc -o test test.c $DIR/libmthpc.a $CFLAGS
./test
#rm -f test
