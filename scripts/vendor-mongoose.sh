#!/bin/bash
# Vendor mongoose source files from GitHub
TAG="7.21"
BASE="https://raw.githubusercontent.com/cesanta/mongoose/$TAG"
OUT="vendor/mongoose"

mkdir -p "$OUT"

echo "Fetching mongoose $TAG..."

curl -sL "$BASE/mongoose.c" -o "$OUT/mongoose.c"
curl -sL "$BASE/mongoose.h" -o "$OUT/mongoose.h"

echo "Done: mongoose $TAG vendored to $OUT"
