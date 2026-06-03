#!/bin/sh
set -e

VERSION="@@VERSION@@"
PREFIX="/opt/sparkd"
BIN_LINK_DIR="/usr/local/bin"

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --prefix=*) PREFIX="${arg#--prefix=}" ;;
        --help|-h)
            echo "Usage: ./install.sh [--prefix=/opt/sparkd]"
            echo "  Installs sparkd binaries and UI to PREFIX."
            echo "  Default: /opt/sparkd"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Usage: ./install.sh [--prefix=/opt/sparkd]"
            exit 1
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Determine if we need sudo
SUDO=""
if [ ! -w "$(dirname "$PREFIX")" ]; then
    SUDO="sudo"
fi

echo ""
echo "  sparkd installer (v$VERSION)"
echo "  ========================"
echo ""
echo "  Install to: $PREFIX"
echo "  Symlinks:   $BIN_LINK_DIR"
if [ -n "$SUDO" ]; then
    echo "  Privilege:  sudo (target not user-writable)"
fi
echo ""
printf "  Press ENTER to install, or Ctrl+C to cancel..."
read _
echo ""

# Copy files
echo "[1/3] Installing files..."
$SUDO mkdir -p "$PREFIX/bin" "$PREFIX/ui"
$SUDO cp "$SCRIPT_DIR/bin/"* "$PREFIX/bin/"
$SUDO chmod 755 "$PREFIX/bin/"*
$SUDO cp -r "$SCRIPT_DIR/ui/"* "$PREFIX/ui/"
echo "  Installed to $PREFIX"

# Symlinks
echo ""
echo "[2/3] Creating symlinks..."
$SUDO mkdir -p "$BIN_LINK_DIR"
for bin in "$PREFIX/bin/"*; do
    name="$(basename "$bin")"
    $SUDO ln -sf "$bin" "$BIN_LINK_DIR/$name"
    echo "  $BIN_LINK_DIR/$name -> $bin"
done

# Verify
echo ""
echo "[3/3] Verifying..."
if command -v sparkctl >/dev/null 2>&1; then
    echo "  sparkctl found in PATH"
else
    echo "  WARNING: $BIN_LINK_DIR not in PATH, add it to your shell profile"
fi

echo ""
echo "  ========================"
echo "  sparkd v$VERSION installed successfully!"
echo "  Run: sparkctl --help"
echo ""
