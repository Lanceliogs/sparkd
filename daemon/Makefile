# sparkd daemon Makefile

CC      ?= gcc
CFLAGS  := -Wall -Wextra -pedantic -std=c17 -g -Isrc -I../vendor/portmidi
LDFLAGS :=

BUILDDIR := ../build
EXE      :=
VENDORDIR := ../vendor/portmidi

# Platform detection
ifeq ($(OS),Windows_NT)
  EXE      := .exe
  LDFLAGS  += -lws2_32 -lwinmm
  PM_CFLAGS := -DWIN32
  PM_SRCS  := $(VENDORDIR)/portmidi.c $(VENDORDIR)/pmutil.c \
              $(VENDORDIR)/pmwin.c $(VENDORDIR)/pmwinmm.c \
              $(VENDORDIR)/ptwinmm.c
else
  CFLAGS   += -D_DEFAULT_SOURCE -DPMALSA
  LDFLAGS  += -lpthread -lasound
  PM_CFLAGS := -DPMALSA 
  PM_SRCS  := $(VENDORDIR)/portmidi.c $(VENDORDIR)/pmutil.c \
              $(VENDORDIR)/pmlinux.c $(VENDORDIR)/pmlinuxalsa.c \
              $(VENDORDIR)/ptlinux.c
endif

TARGET := $(BUILDDIR)/sparkd$(EXE)

# Source files (add as modules are implemented)
SRCS := src/main.c src/log.c src/clock.c
SRCS += src/scene.c src/stage.c src/dmx/dmx_out.c src/serial/serial.c
SRCS += src/dmx/dmx.c src/dmx/dmx_dummy.c src/dmx/dmx_open.c

OBJS := $(SRCS:src/%.c=$(BUILDDIR)/daemon/%.o)

# PortMidi objects
PM_OBJS := $(PM_SRCS:$(VENDORDIR)/%.c=$(BUILDDIR)/vendor/portmidi/%.o)

# Test files
TEST_SRCS := $(wildcard tests/test_*.c)
TEST_BINS := $(TEST_SRCS:tests/test_%.c=$(BUILDDIR)/tests/test_%$(EXE))

# Modules without main (for linking into tests)
LIB_SRCS := $(filter-out src/main.c, $(SRCS))
LIB_OBJS := $(LIB_SRCS:src/%.c=$(BUILDDIR)/daemon/%.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS) $(PM_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(PM_OBJS) $(LDFLAGS)

$(BUILDDIR)/daemon/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/vendor/portmidi/%.o: $(VENDORDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c17 -g -D_DEFAULT_SOURCE -I$(VENDORDIR) $(PM_CFLAGS) -c $< -o $@

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo "--- $$t ---"; \
		$$t || exit 1; \
	done

$(BUILDDIR)/tests/test_%$(EXE): tests/test_%.c $(LIB_OBJS) $(PM_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itests -o $@ $< $(LIB_OBJS) $(PM_OBJS) $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR)/daemon $(BUILDDIR)/tests $(BUILDDIR)/vendor $(TARGET)
