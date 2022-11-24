PWD := $(CURDIR)

INC=$(PWD)/include
INC_PARAMS=$(INC:%=-I%)

CC ?= gcc
CFLAGS:=-g 
CFLAGS+=-Wall
CFLAGS+=-O1
CFLAGS+=-rdynamic

SRC:=src/centralized_barrier/centralized_barrier.c
SRC+=src/workqueue/workqueue.c
#SRC+=src/mlrcu/mlrcu.c

OBJ:=$(SRC:.c=.o)

STATIC_BIN=libmthpc.a

%.o: %.c
	$(CC) $(CFLAGS) $(INC_PARAMS) -c $< -o $@

static: $(OBJ)
	ar crsv $(STATIC_BIN) $(OBJ)
	ranlib  $(STATIC_BIN)

clean:
	rm -f src/*/*.o
	rm -f $(STATIC_BIN)

indent:
	clang-format -i include/*.[ch]
	clang-format -i include/*/*.[ch]
	clang-format -i src/*/*.[ch]
