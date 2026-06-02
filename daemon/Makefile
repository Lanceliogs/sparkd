# sparkd daemon Makefile

CC      ?= gcc
CFLAGS  := -Wall -Wextra -pedantic -std=c17 -g -Isrc -I../vendor/portmidi -I../vendor/mongoose -I../vendor/libyaml
LDFLAGS :=

BUILDDIR := ../build
EXE      :=
PORTMIDIDIR := ../vendor/portmidi
MONGOOSEDIR := ../vendor/mongoose
YAMLDIR     := ../vendor/libyaml
CRESDIR     := ../vendor/cres
RESDIR      := $(BUILDDIR)/resources

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

# Shared flags for vendored libraries (no warnings, C17)
VENDOR_CFLAGS := -std=c17 -g -D_DEFAULT_SOURCE -w

YAML_CFLAGS := -DYAML_DECLARE_STATIC \
  -DYAML_VERSION_STRING=\"0.2.5\" -DYAML_VERSION_MAJOR=0 \
  -DYAML_VERSION_MINOR=2 -DYAML_VERSION_PATCH=5

# ---- Daemon sources ----

SRCS := src/main.c src/log.c src/clock.c src/env.c src/engine.c src/http.c src/shutdown.c src/auth.c
SRCS += src/project/project.c src/project/project_yaml.c
SRCS += src/midi.c src/scene.c src/stage.c src/fixture.c src/fixture_bank.c src/dmx/dmx_out.c
SRCS += src/serial/serial_common.c $(SERIAL_SRCS)
SRCS += src/dmx/dmx.c src/dmx/dmx_dummy.c src/dmx/dmx_open.c src/dmx/dmx_pro.c

OBJS := $(SRCS:src/%.c=$(BUILDDIR)/daemon/%.o)

LIB_SRCS := $(filter-out src/main.c, $(SRCS))
LIB_OBJS := $(LIB_SRCS:src/%.c=$(BUILDDIR)/daemon/%.o)

# Reusable object groups
ENV_OBJS  := $(BUILDDIR)/daemon/env.o $(BUILDDIR)/daemon/log.o
CLOCK_OBJ := $(BUILDDIR)/daemon/clock.o
AUTH_OBJ  := $(BUILDDIR)/daemon/auth.o

# ---- Vendor objects ----

PM_OBJS   := $(PM_SRCS:$(PORTMIDIDIR)/%.c=$(BUILDDIR)/vendor/portmidi/%.o)
MG_OBJ    := $(BUILDDIR)/vendor/mongoose/mongoose.o

YAML_SRCS := $(YAMLDIR)/api.c $(YAMLDIR)/reader.c $(YAMLDIR)/scanner.c \
             $(YAMLDIR)/parser.c $(YAMLDIR)/loader.c $(YAMLDIR)/writer.c \
             $(YAMLDIR)/emitter.c $(YAMLDIR)/dumper.c
YAML_OBJS := $(YAML_SRCS:$(YAMLDIR)/%.c=$(BUILDDIR)/vendor/libyaml/%.o)

CRES_OBJ  := $(BUILDDIR)/vendor/cres/cres.o
MINIZ_OBJ := $(BUILDDIR)/vendor/cres/miniz.o

# ---- Resource embedding ----

CRES_TOOL  := $(RESDIR)/cres$(EXE)
UI_RES_SRC := $(RESDIR)/ui_resources.c
UI_RES_HDR := $(RESDIR)/ui_resources.h
UI_RES_OBJ := $(RESDIR)/ui_resources.o

# ---- Tools ----

ALL_TOOLS := $(BUILDDIR)/tools/sparkctl$(EXE) \
             $(BUILDDIR)/tools/spark-midi$(EXE) \
             $(BUILDDIR)/tools/spark-serial$(EXE) \
             $(BUILDDIR)/tools/spark-ui$(EXE) \
             $(BUILDDIR)/tools/spark-reaper$(EXE)

UI_SRCS := tools/ui/spark_ui.c tools/ui/editor_http.c tools/ui/editor.c tools/ui/editor_yaml.c tools/ui/editor_places.c
UI_OBJS := $(UI_SRCS:tools/ui/%.c=$(BUILDDIR)/tools/ui/%.o)
EDITOR_OBJS := $(BUILDDIR)/tools/ui/editor.o $(BUILDDIR)/tools/ui/editor_yaml.o
SERIAL_OBJS := $(BUILDDIR)/daemon/serial/serial_common.o $(SERIAL_SRCS:src/%.c=$(BUILDDIR)/daemon/%.o)

# ---- Tests ----

TEST_EXCLUDED := tests/test_editor.c tests/test_editor_yaml.c \
                 tests/test_auth.c tests/test_sparkctl_helpers.c tests/test_cres.c
TEST_SRCS := $(filter-out $(TEST_EXCLUDED), $(wildcard tests/test_*.c))
TEST_BINS := $(TEST_SRCS:tests/test_%.c=$(BUILDDIR)/tests/test_%$(EXE))

EDITOR_TEST_BINS := $(BUILDDIR)/tests/test_editor$(EXE) $(BUILDDIR)/tests/test_editor_yaml$(EXE)
CUSTOM_TEST_BINS := $(BUILDDIR)/tests/test_auth$(EXE) \
                    $(BUILDDIR)/tests/test_sparkctl_helpers$(EXE) \
                    $(BUILDDIR)/tests/test_cres$(EXE)

# ============================================================================
# Targets
# ============================================================================

TARGET := $(BUILDDIR)/sparkd$(EXE)

.PHONY: all clean test tools

all: $(TARGET)

tools: $(ALL_TOOLS)

# ---- Daemon ----

$(TARGET): $(OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS) $(LDFLAGS)

$(BUILDDIR)/daemon/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ---- Vendor Libraries ----

$(BUILDDIR)/vendor/portmidi/%.o: $(PORTMIDIDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(VENDOR_CFLAGS) -I$(PORTMIDIDIR) $(PM_CFLAGS) -c $< -o $@

$(MG_OBJ): $(MONGOOSEDIR)/mongoose.c
	@mkdir -p $(dir $@)
	$(CC) $(VENDOR_CFLAGS) -DMG_ENABLE_LINES=0 -I$(MONGOOSEDIR) -c $< -o $@

$(BUILDDIR)/vendor/libyaml/%.o: $(YAMLDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(VENDOR_CFLAGS) $(YAML_CFLAGS) -I$(YAMLDIR) -c $< -o $@

$(CRES_OBJ): $(CRESDIR)/cres.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -w -DCRES_COMPRESSION -I$(CRESDIR) -c $< -o $@

$(MINIZ_OBJ): $(CRESDIR)/miniz.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -w -DCRES_COMPRESSION -c $< -o $@

# ---- Resource Embedding ----

$(CRES_TOOL): $(CRESDIR)/cres_tool.c $(CRESDIR)/miniz.c
	@mkdir -p $(dir $@)
	$(CC) -std=c11 -w -DCRES_COMPRESSION -o $@ $^

$(UI_RES_SRC) $(UI_RES_HDR): $(CRES_TOOL) ui_resources.txt $(wildcard ../ui/dist/*) $(wildcard ../ui/dist/assets/*)
	@mkdir -p $(RESDIR)
	$(CRES_TOOL) -m ui_resources.txt -o $(RESDIR)/ui_resources

$(UI_RES_OBJ): $(UI_RES_SRC)
	$(CC) -std=c11 -w -DCRES_COMPRESSION -I$(CRESDIR) -c $< -o $@

# ---- Tools ----

$(BUILDDIR)/tools/sparkctl$(EXE): tools/sparkctl.c $(MG_OBJ) $(ENV_OBJS) $(CLOCK_OBJ) $(AUTH_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< $(MG_OBJ) $(ENV_OBJS) $(CLOCK_OBJ) $(AUTH_OBJ) $(LDFLAGS)

$(BUILDDIR)/tools/spark-midi$(EXE): tools/spark_midi.c $(LIB_OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< $(LIB_OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS) $(LDFLAGS)

$(BUILDDIR)/tools/spark-serial$(EXE): tools/spark_serial.c $(SERIAL_OBJS) $(BUILDDIR)/daemon/log.o
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< $(SERIAL_OBJS) $(BUILDDIR)/daemon/log.o $(LDFLAGS)

$(BUILDDIR)/tools/spark-reaper$(EXE): tools/spark_reaper.c $(ENV_OBJS) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $< $(ENV_OBJS) $(YAML_OBJS) $(LDFLAGS)

$(BUILDDIR)/tools/ui/%.o: tools/ui/%.c $(UI_RES_HDR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DSPARK_EMBED_UI -Itools/ui -I$(RESDIR) -I$(CRESDIR) -c $< -o $@

$(BUILDDIR)/tools/spark-ui$(EXE): $(UI_OBJS) $(MG_OBJ) $(ENV_OBJS) $(CLOCK_OBJ) $(AUTH_OBJ) $(YAML_OBJS) $(UI_RES_OBJ) $(CRES_OBJ) $(MINIZ_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itools/ui -I$(RESDIR) -I$(CRESDIR) -o $@ $(UI_OBJS) $(MG_OBJ) $(ENV_OBJS) $(CLOCK_OBJ) $(AUTH_OBJ) $(YAML_OBJS) $(UI_RES_OBJ) $(CRES_OBJ) $(MINIZ_OBJ) $(LDFLAGS)

# ---- Tests ----

test: $(TEST_BINS) $(EDITOR_TEST_BINS) $(CUSTOM_TEST_BINS)
	@for t in $^; do echo "--- $$t ---"; $$t || exit 1; done

$(BUILDDIR)/tests/test_%$(EXE): tests/test_%.c $(LIB_OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itests -o $@ $< $(LIB_OBJS) $(PM_OBJS) $(MG_OBJ) $(YAML_OBJS) $(LDFLAGS)

$(BUILDDIR)/tests/test_editor$(EXE): tests/test_editor.c $(EDITOR_OBJS) $(ENV_OBJS) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itools/ui -Itests -o $@ $< $(EDITOR_OBJS) $(ENV_OBJS) $(YAML_OBJS) $(LDFLAGS)

$(BUILDDIR)/tests/test_editor_yaml$(EXE): tests/test_editor_yaml.c $(EDITOR_OBJS) $(ENV_OBJS) $(YAML_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itools/ui -Itests -o $@ $< $(EDITOR_OBJS) $(ENV_OBJS) $(YAML_OBJS) $(LDFLAGS)

$(BUILDDIR)/tests/test_auth$(EXE): tests/test_auth.c $(AUTH_OBJ) $(ENV_OBJS) $(MG_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itests -o $@ $< $(AUTH_OBJ) $(ENV_OBJS) $(MG_OBJ) $(LDFLAGS)

$(BUILDDIR)/tests/test_sparkctl_helpers$(EXE): tests/test_sparkctl_helpers.c $(MG_OBJ) $(ENV_OBJS) $(CLOCK_OBJ) $(AUTH_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Itests -o $@ $< $(MG_OBJ) $(ENV_OBJS) $(CLOCK_OBJ) $(AUTH_OBJ) $(LDFLAGS)

$(BUILDDIR)/tests/test_cres$(EXE): tests/test_cres.c $(UI_RES_OBJ) $(CRES_OBJ) $(MINIZ_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DSPARK_EMBED_UI -I$(RESDIR) -I$(CRESDIR) -Itests -o $@ $< $(UI_RES_OBJ) $(CRES_OBJ) $(MINIZ_OBJ) $(LDFLAGS)

# ---- Housekeeping ----

clean:
	rm -rf $(BUILDDIR)/daemon $(BUILDDIR)/tests $(BUILDDIR)/tools $(BUILDDIR)/vendor \
	       $(BUILDDIR)/resources $(TARGET)
