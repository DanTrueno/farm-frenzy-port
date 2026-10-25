#!/bin/bash
# Build on Linux (needs a C++17 compiler, CMake and SDL3 build dependencies).
#   scripts/build_linux.sh [path/to/original/game] [path/to/hd.pack]
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release -DFF_GAME_DIR="${1:-}" -DFF_HD_PACK="${2:-}"
cmake --build build-linux -j"$(nproc)"
mkdir -p dist/FarmFrenzy
cp build-linux/FarmFrenzy dist/FarmFrenzy/
[ -d build-linux/Data ] && cp -R build-linux/Data dist/FarmFrenzy/
echo "Built: dist/FarmFrenzy/FarmFrenzy"
