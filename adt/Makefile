
STRUCTURES=tree list radix
INCLUDES = $(foreach dir, $(STRUCTURES),-I $(abspath $(dir)))

CFLAGS=-g -std=c11 $(INCLUDES)

TEST_DIR=test

all: tests

tests: 
	$(MAKE)  -C $(TEST_DIR) CFLAGS="$(CFLAGS)"
