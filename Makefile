PWD := $(CURDIR)

INC=$(PWD)/include
INC_PARAMS=$(INC:%=-I%)

CC ?= gcc
CFLAGS:=-g 
CFLAGS+=-Wall
CFLAGS+=-O1
CFLAGS+=-pthread
CFLAGS+=-fPIC
CFLAGS+=-std=c11

DEBUG_FLAGS=

ifneq ($(strip $(debug)),)
DEBUG_FLAGS+=-fsanitize=thread
DEBUG_FLAGS+=-D'CONFIG_DEBUG'
CFLAGS+=$(DEBUG_FLAGS)
endif

SRC:=src/centralized_barrier/centralized_barrier.c
SRC+=src/rcu/rcu.c
SRC+=src/safe_ptr/safe_ptr.c

SRC+=src/thread/thread.c

SRC+=src/workqueue/workqueue.c
SRC+=src/scoped_lock/scoped_lock.c
#SRC+=src/mlrcu/mlrcu.c

OBJ:=$(SRC:.c=.o)

STATIC_BIN=libmthpc.a
DYNAMIC_BIN=libmthpc.so

ifneq ($(strip $(static)),)
LD=ar
LDFLAGS=crsv
LD_BIN=$(STATIC_BIN)
LD_TO=
LD_GEN=ranlib
LD_GEN_TARGET=$(STATIC_BIN)
BIN=$(STATIC_BIN)
else
LD=$(CC)
LDFLAGS=-shared
LDFLAGS+=$(DEBUG_FLAGS)
LD_BIN=$(DYNAMIC_BIN)
LD_TO=-o
LD_GEN=
LD_GEN_TARGET=
BIN=$(DYNAMIC_BIN)
endif

BUILD_DIR=mthpc

ifeq ($(quiet),1)

MTHPC_CC=@$(CC)
MTHPC_LD=@$(LD)
MTHPC_LD_GEN=@$(LD_GEN)
MTHPC_RM=@rm
MTHPC_MKDIR=@mkdir
MTHPC_CP=@cp
MTHPC_MV=@mv

else

MTHPC_CC=$(CC)
MTHPC_LD=$(LD)
MTHPC_LD_GEN=$(LD_GEN)
MTHPC_RM=rm
MTHPC_MKDIR=mkdir
MTHPC_CP=cp
MTHPC_MV=mv

endif

%.o: %.c
	$(MTHPC_CC) $(CFLAGS) $(INC_PARAMS) -c $< -o $@

lib: $(OBJ)
	$(MTHPC_LD) $(LDFLAGS) $(LD_TO) $(LD_BIN) $(OBJ)
	$(MTHPC_LD_GEN) $(LD_GEN_TARGET)

build: lib
	$(MTHPC_RM) -rf $(BUILD_DIR)
	$(MTHPC_MKDIR) $(BUILD_DIR)
	$(MTHPC_MKDIR) $(BUILD_DIR)/include
	$(MTHPC_CP) -r include/mthpc $(BUILD_DIR)/include/.
	$(MTHPC_MV) $(BIN) $(BUILD_DIR)/.

clean:
	$(MTHPC_RM) -f src/*/*.o
	$(MTHPC_RM) -f $(STATIC_BIN)
	$(MTHPC_RM) -f $(DYNAMIC_BIN)

cleanall: clean
	$(MTHPC_RM) -rf $(BUILD_DIR)

cscope:
	find $(PWD) -name "*.c" -o -name "*.h" > $(PWD)/cscope.files
	cscope -b -q

indent:
	clang-format -i include/*/*.[ch]
	clang-format -i src/*/*.[ch]

ifeq ($(quiet), 1)
.SILENT:
endif


