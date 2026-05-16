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
  CFLAGS   += -D_DEFAULT_SOURCE
  LDFLAGS  += -lpthread
endif

TARGET := $(BUILDDIR)/sparkd$(EXE)

# Source files (add as modules are implemented)
SRCS := src/main.c src/log.c src/clock.c src/dmx/dmx.c src/dmx/dmx_dummy.c src/stage.c src/dmx/dmx_out.c src/serial/serial.c
OBJS := $(SRCS:src/%.c=$(BUILDDIR)/daemon/%.o)

# Test files
TEST_SRCS := $(wildcard tests/test_*.c)
TEST_BINS := $(TEST_SRCS:tests/test_%.c=$(BUILDDIR)/tests/test_%$(EXE))

# Modules without main (for linking into tests)
LIB_SRCS := $(filter-out src/main.c, $(SRCS))
LIB_OBJS := $(LIB_SRCS:src/%.c=$(BUILDDIR)/daemon/%.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(BUILDDIR)/daemon/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo "--- $$t ---"; \
		$$t || exit 1; \
	done

$(BUILDDIR)/tests/test_%$(EXE): tests/test_%.c $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itests -o $@ $< $(LIB_OBJS) $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR)/daemon $(BUILDDIR)/tests $(TARGET)
