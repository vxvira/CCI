#!/usr/bin/env bash
# Build and run a source file (default src/main.cpp) against the vendored AZBacktest amalgamation.
# Usage: ./build.sh [src/optimizations/tpsl.cpp]
# Requires: brew install glfw
set -e
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BREW=/opt/homebrew; [[ -d $BREW/lib ]] || BREW="$(brew --prefix)"   # prefer the arm64 Homebrew
SRC="$(cd "$(dirname "${1:-$ROOT/src/main.cpp}")" && pwd)/$(basename "${1:-main.cpp}")"
OUT="$ROOT/build/$(basename "$SRC" .cpp)"
mkdir -p "$ROOT/build"

g++ -std=c++17 -O2 \
    -I"$ROOT/vendor/azbacktest/vendor/eigen" -I"$BREW/include" \
    "$SRC" -o "$OUT" \
    -L"$BREW/lib" -lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo

cd "$ROOT"   # config.toml is read/generated relative to cwd
exec "$OUT"
