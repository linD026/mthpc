BUILD_DIR = $(shell pwd)
CC ?= gcc
CFLAG:=-g 
CFLAG+=-Wall
CFLAG+=-O1

SRC:=mlrcu.c

OBJ=$(SRC:.c=.o)

BIN:=test

%.o: %.c
	$(CC) -c $< $(CFLAG)

all: $(OBJ)
	$(CC) -o $(BIN) $(OBJ) $(CFLAG)

clean:
	rm -f $(BIN)

indent:
	clang-format -i *.[ch]
