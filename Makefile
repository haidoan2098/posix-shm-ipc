CC      := gcc
AR      := ar
CFLAGS  := -std=gnu11 -Wall -Wextra -O2 -g -Iinclude
SRC     := src
BIN     := build

LIB     := $(BIN)/libshm_vku.a
PROGS   := $(foreach t,test1 test2 test3,$(BIN)/$(t)/writer $(BIN)/$(t)/reader)

all: $(LIB) $(PROGS)

$(BIN):
	mkdir -p $@

$(BIN)/%.o: $(SRC)/%.c include/shm_vku.h | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(BIN)/shm_vku.o
	$(AR) rcs $@ $^

# build/test1/writer is built from test/test1/writer.c, and so on
$(PROGS): $(BIN)/%: test/%.c $(LIB)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf $(BIN)

.PHONY: all clean
