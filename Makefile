CC      := gcc
AR      := ar
CFLAGS  := -std=gnu11 -Wall -Wextra -O2 -g -Iinclude
SRC     := src
BIN     := build

LIB     := $(BIN)/libshm_vku.a
TESTS   := $(BIN)/test_one_writer_one_reader \
	   $(BIN)/test_two_writers_one_reader

all: $(LIB)

$(BIN):
	mkdir -p $@

$(BIN)/%.o: $(SRC)/%.c include/shm_vku.h | $(BIN)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(BIN)/shm_vku.o
	$(AR) rcs $@ $^

$(BIN)/test_%: test/test_%.c $(LIB)
	$(CC) $(CFLAGS) $^ -o $@

test: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

clean:
	rm -rf $(BIN)

.PHONY: all test clean
