# sparkd daemon Makefile

CC      ?= gcc
CFLAGS  := -Wall -Wextra -pedantic -std=c17 -g -Isrc
LDFLAGS :=

BUILDDIR := ../build
EXE      :=

# Platform detection
ifeq ($(OS),Windows_NT)
  EXE      := .exe
  LDFLAGS  += -lws2_32 -lwinmm
else
  LDFLAGS  += -lpthread
endif

TARGET := $(BUILDDIR)/sparkd$(EXE)

# Source files (add as modules are implemented)
SRCS := src/main.c
OBJS := $(SRCS:src/%.c=$(BUILDDIR)/daemon/%.o)

# Test files
TEST_SRCS := $(wildcard tests/test_*.c)
TEST_BINS := $(TEST_SRCS:tests/test_%.c=$(BUILDDIR)/tests/test_%$(EXE))

# Modules without main (for linking into tests)
LIB_SRCS := $(filter-out src/main.c, $(SRCS))
LIB_OBJS := $(LIB_SRCS:src/%.c=$(BUILDDIR)/daemon/%.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(BUILDDIR)/daemon/%.o: src/%.c | $(BUILDDIR)/daemon
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/daemon:
	mkdir -p $(BUILDDIR)/daemon

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo "--- $$t ---"; \
		$$t || exit 1; \
	done

$(BUILDDIR)/tests/test_%$(EXE): tests/test_%.c $(LIB_OBJS) | $(BUILDDIR)/tests
	$(CC) $(CFLAGS) -Itests -o $@ $< $(LIB_OBJS) $(LDFLAGS)

$(BUILDDIR)/tests:
	mkdir -p $(BUILDDIR)/tests

clean:
	rm -rf $(BUILDDIR)/daemon $(BUILDDIR)/tests $(TARGET)
