# sparkctl Makefile

CC      ?= gcc
CFLAGS  := -Wall -Wextra -pedantic -std=c17 -g
LDFLAGS :=

BUILDDIR := ../build
TARGET   := $(BUILDDIR)/sparkctl

ifeq ($(OS),Windows_NT)
  TARGET   := $(BUILDDIR)/sparkctl.exe
  LDFLAGS  += -lws2_32
endif

SRCS :=
OBJS := $(SRCS:%.c=$(BUILDDIR)/tools/%.o)

.PHONY: all clean

all:
ifneq ($(SRCS),)
all: $(TARGET)
endif

$(TARGET): $(OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(BUILDDIR)/tools/%.o: %.c | $(BUILDDIR)/tools
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/tools:
	mkdir -p $(BUILDDIR)/tools

clean:
	rm -rf $(BUILDDIR)/tools $(TARGET)
