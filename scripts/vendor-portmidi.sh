#!/bin/bash
# Vendor portmidi source files from GitHub
TAG="v2.0.7"
BASE="https://raw.githubusercontent.com/PortMidi/portmidi/$TAG"
OUT="vendor/portmidi"

mkdir -p "$OUT"

echo "Fetching portmidi $TAG..."

# Core
curl -sL "$BASE/pm_common/portmidi.h"    -o "$OUT/portmidi.h"
curl -sL "$BASE/pm_common/portmidi.c"    -o "$OUT/portmidi.c"
curl -sL "$BASE/pm_common/pmutil.h"      -o "$OUT/pmutil.h"
curl -sL "$BASE/pm_common/pmutil.c"      -o "$OUT/pmutil.c"
curl -sL "$BASE/pm_common/pminternal.h"  -o "$OUT/pminternal.h"

# Windows
curl -sL "$BASE/pm_win/pmwin.c"    -o "$OUT/pmwin.c"
curl -sL "$BASE/pm_win/pmwinmm.c"  -o "$OUT/pmwinmm.c"
curl -sL "$BASE/pm_win/pmwinmm.h"  -o "$OUT/pmwinmm.h"

# Linux
curl -sL "$BASE/pm_linux/pmlinux.c"      -o "$OUT/pmlinux.c"
curl -sL "$BASE/pm_linux/pmlinuxalsa.c"  -o "$OUT/pmlinuxalsa.c"
curl -sL "$BASE/pm_linux/pmlinuxalsa.h"  -o "$OUT/pmlinuxalsa.h"

# Timer (porttime)
curl -sL "$BASE/porttime/porttime.h"  -o "$OUT/porttime.h"
curl -sL "$BASE/porttime/porttime.c"  -o "$OUT/porttime.c"
curl -sL "$BASE/porttime/ptwinmm.c"   -o "$OUT/ptwinmm.c"
curl -sL "$BASE/porttime/ptlinux.c"   -o "$OUT/ptlinux.c"

echo "Done: portmidi $TAG vendored to $OUT"
