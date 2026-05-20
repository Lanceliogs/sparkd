# sparkd daemon Makefile

CC      ?= gcc
CFLAGS  := -Wall -Wextra -pedantic -std=c17 -g -Isrc -I../vendor/portmidi -I../vendor/mongoose -I../vendor/libyaml
LDFLAGS :=

BUILDDIR := ../build
EXE      :=
PORTMIDIDIR   := ../vendor/portmidi
MONGOOSEDIR := ../vendor/mongoose
YAMLDIR     := ../vendor/libyaml

# Platform detection
ifeq ($(OS),Windows_NT)
  EXE      := .exe
  LDFLAGS  += -lws2_32 -lwinmm
  PM_CFLAGS := -DWIN32
  PM_SRCS  := $(PORTMIDIDIR)/portmidi.c $(PORTMIDIDIR)/pmutil.c \
              $(PORTMIDIDIR)/pmwin.c $(PORTMIDIDIR)/pmwinmm.c \
              $(PORTMIDIDIR)/ptwinmm.c
else
  CFLAGS   += -D_DEFAULT_SOURCE -DPMALSA
  LDFLAGS  += -lpthread -lasound
  PM_CFLAGS := -DPMALSA 
  PM_SRCS  := $(PORTMIDIDIR)/portmidi.c $(PORTMIDIDIR)/pmutil.c \
              $(PORTMIDIDIR)/pmlinux.c $(PORTMIDIDIR)/pmlinuxalsa.c \
              $(PORTMIDIDIR)/ptlinux.c
endif

TARGET := $(BUILDDIR)/sparkd$(EXE)

# Source files (add as modules are implemented)
SRCS := src/main.c src/log.c src/clock.c src/engine.c src/project.c src/http.c
SRCS += src/midi.c src/scene.c src/stage.c src/fixture.c src/dmx/dmx_out.c src/serial/serial.c
SRCS += src/dmx/dmx.c src/dmx/dmx_dummy.c src/dmx/dmx_open.c

OBJS := $(SRCS:src/%.c=$(BUILDDIR)/daemon/%.o)

# PortMidi objects
PM_OBJS := $(PM_SRCS:$(PORTMIDIDIR)/%.c=$(BUILDDIR)/vendor/portmidi/%.o)

# Mongoose object
MG_OBJ := $(BUILDDIR)/vendor/mongoose/mongoose.o

# libyaml objects
YAML_SRCS := $(YAMLDIR)/api.c $(YAMLDIR)/reader.c $(YAMLDIR)/scanner.c \
             $(YAMLDIR)/parser.c $(YAMLDIR)/loader.c $(YAMLDIR)/writer.c \
             $(YAMLDIR)/emitter.c $(YAMLDIR)/dumper.c
YAML_OBJS := $(YAML_SRCS:$(YAMLDIR)/%.c=$(BUILDDIR)/vendor/libyaml/%.o)

# Test files
TEST_SRCS := $(wildcard tests/test_*.c)
TEST_BINS := $(TEST_SRCS:tests/test_%.c=$(BUILDDIR)/tests/test_%$(EXE))

# Modules without main (for linking into tests)
LIB_SRCS := $(filter-out src/main.c, $(SRCS))
LIB_OBJS := $(LIB_SRCS:src/%.c=$(BUILDDIR)/daemon/%.o)

# Tools (linked against daemon libs)
TOOL_SRCS := $(filter-out tools/sparkctl.c, $(wildcard tools/*.c))
TOOL_BINS := $(TOOL_SRCS:tools/%.c=$(BUILDDIR)/tools/%$(EXE))

# Standalone tools (no daemon deps)
STANDALONE_TOOLS := $(BUILDDIR)/tools/sparkctl$(EXE)

.PHONY: all clean test tools

all: $(TARGET)

tools: $(TOOL_BINS) $(STANDALONE_TOOLS)

$(TARGET): $(OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS) $(LDFLAGS)

$(BUILDDIR)/daemon/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/vendor/portmidi/%.o: $(PORTMIDIDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c17 -g -D_DEFAULT_SOURCE -I$(PORTMIDIDIR) $(PM_CFLAGS) -c $< -o $@

$(MG_OBJ): $(MONGOOSEDIR)/mongoose.c
	@mkdir -p $(dir $@)
	$(CC) -std=c17 -g -D_DEFAULT_SOURCE -DMG_ENABLE_LINES=0 -I$(MONGOOSEDIR) -w -c $< -o $@

$(BUILDDIR)/vendor/libyaml/%.o: $(YAMLDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=c17 -g -D_DEFAULT_SOURCE -DYAML_DECLARE_STATIC \
	  -DYAML_VERSION_STRING=\"0.2.5\" -DYAML_VERSION_MAJOR=0 \
	  -DYAML_VERSION_MINOR=2 -DYAML_VERSION_PATCH=5 \
	  -I$(YAMLDIR) -w -c $< -o $@

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo "--- $$t ---"; \
		$$t || exit 1; \
	done

$(BUILDDIR)/tests/test_%$(EXE): tests/test_%.c $(LIB_OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itests -o $@ $< $(LIB_OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS) $(LDFLAGS)

$(BUILDDIR)/tools/%$(EXE): tools/%.c $(LIB_OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< $(LIB_OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS) $(LDFLAGS)

$(BUILDDIR)/tools/sparkctl$(EXE): tools/sparkctl.c $(MG_OBJ)
	@mkdir -p $(dir $@)
	$(CC) -Wall -Wextra -pedantic -std=c17 -g -D_DEFAULT_SOURCE -I$(MONGOOSEDIR) -o $@ $< $(MG_OBJ) $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR)/daemon $(BUILDDIR)/tests $(BUILDDIR)/tools $(BUILDDIR)/vendor $(TARGET)
