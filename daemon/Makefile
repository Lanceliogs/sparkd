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
  LDFLAGS  += -static -lws2_32 -lwinmm -lsetupapi
  PM_CFLAGS := -DWIN32
  PM_SRCS  := $(PORTMIDIDIR)/portmidi.c $(PORTMIDIDIR)/pmutil.c \
              $(PORTMIDIDIR)/pmwin.c $(PORTMIDIDIR)/pmwinmm.c \
              $(PORTMIDIDIR)/ptwinmm.c
  SERIAL_SRCS := src/serial/serial_win32.c
else
  CFLAGS   += -D_DEFAULT_SOURCE -DPMALSA
  LDFLAGS  += -lpthread -lasound
  PM_CFLAGS := -DPMALSA 
  PM_SRCS  := $(PORTMIDIDIR)/portmidi.c $(PORTMIDIDIR)/pmutil.c \
              $(PORTMIDIDIR)/pmlinux.c $(PORTMIDIDIR)/pmlinuxalsa.c \
              $(PORTMIDIDIR)/ptlinux.c
  SERIAL_SRCS := src/serial/serial_posix.c
endif

TARGET := $(BUILDDIR)/sparkd$(EXE)

# Source files (add as modules are implemented)
SRCS := src/main.c src/log.c src/clock.c src/env.c src/engine.c src/http.c
SRCS += src/project/project.c src/project/project_yaml.c
SRCS += src/midi.c src/scene.c src/stage.c src/fixture.c src/fixture_bank.c src/dmx/dmx_out.c
SRCS += src/serial/serial_common.c $(SERIAL_SRCS)
SRCS += src/dmx/dmx.c src/dmx/dmx_dummy.c src/dmx/dmx_open.c src/dmx/dmx_pro.c

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
TEST_SRCS := $(filter-out tests/test_editor.c tests/test_editor_yaml.c, $(wildcard tests/test_*.c))
TEST_BINS := $(TEST_SRCS:tests/test_%.c=$(BUILDDIR)/tests/test_%$(EXE))

# Editor test files (need UI objects instead of daemon LIB_OBJS)
EDITOR_TEST_SRCS := tests/test_editor.c tests/test_editor_yaml.c
EDITOR_TEST_BINS := $(EDITOR_TEST_SRCS:tests/%.c=$(BUILDDIR)/tests/%$(EXE))

# Modules without main (for linking into tests)
LIB_SRCS := $(filter-out src/main.c, $(SRCS))
LIB_OBJS := $(LIB_SRCS:src/%.c=$(BUILDDIR)/daemon/%.o)

# Tools (linked against daemon libs)
TOOL_SRCS := $(filter-out tools/sparkctl.c tools/midi_list.c tools/midi_monitor.c tools/spark_midi.c tools/spark_serial.c, $(wildcard tools/*.c))
TOOL_BINS := $(TOOL_SRCS:tools/%.c=$(BUILDDIR)/tools/%$(EXE))

# Standalone tools (no daemon deps)
STANDALONE_TOOLS := $(BUILDDIR)/tools/sparkctl$(EXE) $(BUILDDIR)/tools/spark-midi$(EXE) $(BUILDDIR)/tools/spark-serial$(EXE) $(BUILDDIR)/tools/spark-ui$(EXE) $(BUILDDIR)/tools/spark-reaper$(EXE)

# Editor UI objects
UI_SRCS := tools/ui/spark_ui.c tools/ui/editor_http.c tools/ui/editor.c tools/ui/editor_yaml.c tools/ui/editor_places.c
UI_OBJS := $(UI_SRCS:tools/ui/%.c=$(BUILDDIR)/tools/ui/%.o)
EDITOR_OBJS := $(BUILDDIR)/tools/ui/editor.o $(BUILDDIR)/tools/ui/editor_yaml.o

ENV_OBJS := $(BUILDDIR)/daemon/env.o $(BUILDDIR)/daemon/log.o

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

test: $(TEST_BINS) $(EDITOR_TEST_BINS)
	@for t in $(TEST_BINS) $(EDITOR_TEST_BINS); do \
		echo "--- $$t ---"; \
		$$t || exit 1; \
	done

$(BUILDDIR)/tests/test_%$(EXE): tests/test_%.c $(LIB_OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itests -o $@ $< $(LIB_OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS) $(LDFLAGS)

$(BUILDDIR)/tests/test_editor$(EXE): tests/test_editor.c $(EDITOR_OBJS) $(ENV_OBJS) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itools/ui -Itests -o $@ $< $(EDITOR_OBJS) $(ENV_OBJS) $(YAML_OBJS) $(LDFLAGS)

$(BUILDDIR)/tests/test_editor_yaml$(EXE): tests/test_editor_yaml.c $(EDITOR_OBJS) $(ENV_OBJS) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itools/ui -Itests -o $@ $< $(EDITOR_OBJS) $(ENV_OBJS) $(YAML_OBJS) $(LDFLAGS)

$(BUILDDIR)/tools/%$(EXE): tools/%.c $(LIB_OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< $(LIB_OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS) $(LDFLAGS)

$(BUILDDIR)/tools/sparkctl$(EXE): tools/sparkctl.c $(MG_OBJ) $(ENV_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< $(MG_OBJ) $(ENV_OBJS) $(LDFLAGS)

$(BUILDDIR)/tools/spark-midi$(EXE): tools/spark_midi.c $(LIB_OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< $(LIB_OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS) $(LDFLAGS)

SERIAL_OBJS := $(BUILDDIR)/daemon/serial/serial_common.o $(SERIAL_SRCS:src/%.c=$(BUILDDIR)/daemon/%.o)

$(BUILDDIR)/tools/spark-serial$(EXE): tools/spark_serial.c $(SERIAL_OBJS) $(BUILDDIR)/daemon/log.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< $(SERIAL_OBJS) $(BUILDDIR)/daemon/log.o $(LDFLAGS)

$(BUILDDIR)/tools/spark-reaper$(EXE): tools/spark_reaper.c $(ENV_OBJS) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< $(ENV_OBJS) $(YAML_OBJS) $(LDFLAGS)

$(BUILDDIR)/tools/ui/%.o: tools/ui/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itools/ui -c $< -o $@

$(BUILDDIR)/tools/spark-ui$(EXE): $(UI_OBJS) $(MG_OBJ) $(ENV_OBJS) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itools/ui -o $@ $(UI_OBJS) $(MG_OBJ) $(ENV_OBJS) $(YAML_OBJS) $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR)/daemon $(BUILDDIR)/tests $(BUILDDIR)/tools $(BUILDDIR)/vendor $(TARGET)
