#!/usr/bin/env bash
# Build and run a source file (default src/main.cpp) against the vendored AZBacktest amalgamation.
# Usage: ./build.sh [src/optimizations/tpsl.cpp]
# Requires: brew install glfw (macOS), or MSYS2 mingw-w64-x86_64-glfw (Windows)
set -e
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$(cd "$(dirname "${1:-$ROOT/src/main.cpp}")" && pwd)/$(basename "${1:-main.cpp}")"
OUT="$ROOT/build/$(basename "$SRC" .cpp)"
mkdir -p "$ROOT/build"

if [[ "$OSTYPE" == msys* || "$OSTYPE" == cygwin* ]]; then
    g++ -std=c++17 -O2 \
        -I"$ROOT/vendor/azbacktest/vendor/eigen" \
        "$SRC" -o "$OUT" \
        -lglfw3 -lopengl32 -lgdi32 -lshell32 -lwinmm -limm32 \
        -static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
else
    BREW=/opt/homebrew; [[ -d $BREW/lib ]] || BREW="$(brew --prefix)"   # prefer the arm64 Homebrew
    g++ -std=c++17 -O2 \
        -I"$ROOT/vendor/azbacktest/vendor/eigen" -I"$BREW/include" \
        "$SRC" -o "$OUT" \
        -L"$BREW/lib" -lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
fi

cd "$ROOT"   # config.toml is read/generated relative to cwd
exec "$OUT"
