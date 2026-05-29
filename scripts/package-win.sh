#!/bin/bash
set -e

# Use mingw32-make if make isn't available
if command -v make &> /dev/null; then
    MAKE=make
elif command -v mingw32-make &> /dev/null; then
    MAKE=mingw32-make
else
    echo "ERROR: No make tool found (tried make, mingw32-make)"
    exit 1
fi

VERSION=$(grep 'SPARKD_VERSION' daemon/src/consts.h | sed 's/.*"\(.*\)".*/\1/')
OUTDIR="sparkd-v${VERSION}-win64"

echo "=== Packaging sparkd v${VERSION} for Windows x64 ==="

# Build
echo "--- Building daemon + tools ---"
$MAKE -C daemon clean
$MAKE -C daemon
$MAKE -C daemon tools

# Build UI
echo "--- Building UI ---"
cd ui && npm run build && cd ..

# Collect
echo "--- Collecting artifacts ---"
rm -rf "$OUTDIR" "${OUTDIR}.zip"
mkdir -p "$OUTDIR"

cp build/sparkd.exe "$OUTDIR/"
cp build/tools/spark-ui.exe "$OUTDIR/"
cp build/tools/spark-midi.exe "$OUTDIR/"
cp build/tools/spark-serial.exe "$OUTDIR/"
cp build/tools/sparkctl.exe "$OUTDIR/"
cp -r ui/dist "$OUTDIR/ui"

# Zip
echo "--- Creating ${OUTDIR}.zip ---"
if command -v 7z &> /dev/null; then
    7z a -tzip "${OUTDIR}.zip" "$OUTDIR"
elif command -v zip &> /dev/null; then
    zip -r "${OUTDIR}.zip" "$OUTDIR"
else
    echo "ERROR: No zip tool found (tried 7z, zip)"
    exit 1
fi

rm -rf "$OUTDIR"
echo "=== Done: ${OUTDIR}.zip ==="
