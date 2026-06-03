#!/bin/sh
set -e

PREFIX="/opt/sparkd"
BIN_LINK_DIR="/usr/local/bin"

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --prefix=*) PREFIX="${arg#--prefix=}" ;;
        --help|-h)
            echo "Usage: ./uninstall.sh [--prefix=/opt/sparkd]"
            exit 0
            ;;
    esac
done

if [ ! -d "$PREFIX" ]; then
    echo "  sparkd is not installed at $PREFIX"
    exit 1
fi

# Determine if we need sudo
SUDO=""
if [ ! -w "$PREFIX" ]; then
    SUDO="sudo"
fi

echo ""
echo "  sparkd uninstaller"
echo "  ========================"
echo ""
echo "  Remove: $PREFIX"
echo "  Unlink: $BIN_LINK_DIR/spark*"
echo ""
printf "  Press ENTER to uninstall, or Ctrl+C to cancel..."
read _
echo ""

# Remove symlinks
echo "[1/2] Removing symlinks..."
for bin in "$PREFIX/bin/"*; do
    name="$(basename "$bin")"
    $SUDO rm -f "$BIN_LINK_DIR/$name"
    echo "  removed $BIN_LINK_DIR/$name"
done

# Remove installation
echo ""
echo "[2/2] Removing $PREFIX..."
$SUDO rm -rf "$PREFIX"

echo ""
echo "  ========================"
echo "  sparkd uninstalled."
echo ""
