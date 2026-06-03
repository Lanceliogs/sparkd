#!/bin/sh
set -e

PREFIX="${SPARKD_PREFIX:-/opt/sparkd}"
BIN_LINK_DIR="/usr/local/bin"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo ""
echo "  sparkd installer"
echo "  ========================"
echo ""
echo "  Install to: $PREFIX"
echo "  Symlinks:   $BIN_LINK_DIR"
echo ""
printf "  Press ENTER to install, or Ctrl+C to cancel..."
read _
echo ""

# Copy files
echo "[1/3] Installing files..."
sudo mkdir -p "$PREFIX/bin" "$PREFIX/ui"
sudo cp "$SCRIPT_DIR/bin/"* "$PREFIX/bin/"
sudo chmod 755 "$PREFIX/bin/"*
sudo cp -r "$SCRIPT_DIR/ui/"* "$PREFIX/ui/"
echo "  Installed to $PREFIX"

# Symlinks
echo ""
echo "[2/3] Creating symlinks..."
sudo mkdir -p "$BIN_LINK_DIR"
for bin in "$PREFIX/bin/"*; do
    name="$(basename "$bin")"
    sudo ln -sf "$bin" "$BIN_LINK_DIR/$name"
    echo "  $BIN_LINK_DIR/$name -> $bin"
done

# Done
echo ""
echo "[3/3] Verifying..."
if command -v sparkctl >/dev/null 2>&1; then
    echo "  sparkctl found in PATH"
else
    echo "  WARNING: $BIN_LINK_DIR not in PATH, add it to your shell profile"
fi

echo ""
echo "  ========================"
echo "  sparkd installed successfully!"
echo "  Run: sparkctl --help"
echo ""
