CC      := gcc
AR      := ar
CFLAGS  := -std=gnu11 -Wall -Wextra -O2 -g -Iinclude
SRC     := src
BIN     := build

LIB     := $(BIN)/libshm_vku.a
PROGS   := $(foreach t,demo1 demo2,$(BIN)/$(t)/writer $(BIN)/$(t)/reader) $(BIN)/demo3/benchmark

all: $(LIB) $(PROGS)

$(BIN):
	mkdir -p $@

$(BIN)/%.o: $(SRC)/%.c include/shm_vku.h | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(BIN)/shm_vku.o
	$(AR) rcs $@ $^

# build/demo1/writer is built from demo/demo1/writer.c, and so on
$(PROGS): $(BIN)/%: demo/%.c $(LIB)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@ -lrt -lpthread

clean:
	rm -rf $(BIN)

.PHONY: all clean
