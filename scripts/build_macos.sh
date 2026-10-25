#!/bin/bash
# Build a self-contained macOS app (universal: Apple Silicon + Intel).
#   scripts/build_macos.sh [path/to/original/game] [path/to/hd.pack]
# The game folder must contain Data/data.pack and Data/ru.pack (and ideally farm.wrp.exe for
# tutorial tips). Without it the app is built empty and looks for Data/ next to itself.
set -euo pipefail
cd "$(dirname "$0")/.."

GAME_DIR="${1:-}"
HD_PACK="${2:-}"
BUILD=build-macos
OUT=dist

cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release \
  -DFF_VENDORED_SDL=ON -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DFF_GAME_DIR="$GAME_DIR" -DFF_HD_PACK="$HD_PACK" -DFF_ICON="${FF_ICON:-}"
cmake --build "$BUILD" -j"$(sysctl -n hw.ncpu)"

mkdir -p "$OUT"
rm -rf "$OUT/Farm Frenzy.app"
cp -R "$BUILD/Farm Frenzy.app" "$OUT/"
codesign --force --deep --sign - "$OUT/Farm Frenzy.app"
echo "Built: $OUT/Farm Frenzy.app"
