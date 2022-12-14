#!/usr/bin/env bash

while getopts "f:i:t:ds" flag
do
    case "${flag}" in
        f) feature=${OPTARG};;
        i) file=${OPTARG};;
        t) tsan_set=${OPTARG};;
        d) debug="1";;
        s) sparse="1";;
    esac
done

###############################################################################

DIR="$(pwd)/../../"
INCLUDE="$DIR/include"

if [ "$debug" = "1" ]; then
CFLAGS="-g -rdynamic -fsanitize=thread -pthread -I$INCLUDE"
else
CFLAGS="-g -rdynamic -pthread -I$INCLUDE"
fi

if [ "$sparse" = "1" ]; then
CC="cgcc"
else
CC="gcc"
fi

LIB="libmthpc.so"

compile="$CC -o test $file $LIB $CFLAGS"

function exec_prog {
    if [ "$tsan_set" = "nope" ]; then
        LD_LIBRARY_PATH=. ./test
    else
        TSAN_OPTIONS="$tsan_set" LD_LIBRARY_PATH=. ./test
    fi
}

###############################################################################

PRFX_GRN='\033[0;32m'
PRFX_NC='\033[0m'
PRFX="[${PRFX_GRN}script${PRFX_NC}]"
echo -e "$PRFX Start testing. feature: $feature"
echo -e "$PRFX file: $file, debug: $debug"
echo -e "$PRFX flags: $CFLAGS"
echo -e "$PRFX tsan options: $tsan_set"
echo -e "$PRFX -----------------------------------------------------------"

echo -e "$PRFX building library..."
make -C $DIR clean quiet=1 --no-print-directory
make -C $DIR debug=1 quiet=1 --no-print-directory
mv $DIR/$LIB .

echo -e "$PRFX compile program..."
$compile

echo -e "$PRFX start executing program..."
exec_prog
echo -e "$PRFX execution end"

echo -e "$PRFX cleanup..."
rm -f test libmthpc.so
rm -rf test.dSYM
make -C $DIR clean quiet=1 --no-print-directory
