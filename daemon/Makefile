# sparkd daemon Makefile

CC      ?= gcc
CFLAGS  := -Wall -Wextra -pedantic -std=c17 -g
LDFLAGS :=

BUILDDIR := ../build
TARGET   := $(BUILDDIR)/sparkd

# Platform detection
ifeq ($(OS),Windows_NT)
  TARGET   := $(BUILDDIR)/sparkd.exe
  LDFLAGS  += -lws2_32 -lwinmm
else
  LDFLAGS  += -lpthread
endif

# Source files (add as modules are implemented)
SRCS := src/main.c
OBJS := $(SRCS:src/%.c=$(BUILDDIR)/daemon/%.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(BUILDDIR)/daemon/%.o: src/%.c | $(BUILDDIR)/daemon
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/daemon:
	mkdir -p $(BUILDDIR)/daemon

clean:
	rm -rf $(BUILDDIR)/daemon $(TARGET)
