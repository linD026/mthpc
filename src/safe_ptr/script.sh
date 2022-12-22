#!/usr/bin/env bash

#TSAN_SET="history_size=5 verbosity=2 flush_memory_ms=20 force_seq_cst_atomics=1"
#TSAN_SET="history_size=5 verbosity=2 force_seq_cst_atomics=1"
#TSAN_SET="force_seq_cst_atomics=1"
TSAN_SET="nope"

bash ../test-setup.sh -d \
                      -f "safe ptr" \
                      -t $TSAN_SET \
                      -i test.c
