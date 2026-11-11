#!/bin/sh
# The prototype for a web browser (WebAssembly), into out/web: index.html,
# lba2menu.js and lba2menu.wasm. The page asks for the player's own game files.
set -e
cd "$(dirname "$0")/.."
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release \
    -DSDL3_DIR="$PWD/cmake/emscripten-sdl3"
cmake --build build-web --target lba2menu -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"
mkdir -p out/web
sed "s/@COMMIT@/$(git rev-parse --short HEAD)$(git diff --quiet HEAD -- . ':!lba2cc' || echo '+changes')/" build-web/lba2menu.html > out/web/index.html
cp build-web/lba2menu.js build-web/lba2menu.wasm out/web/
# The page's faces: Zilla Slab Bold (SIL OFL, with its licence) and lba2cc's 8x8 font.
mkdir -p out/web/fonts
cp assets/fonts/ZillaSlab-Bold.ttf assets/fonts/OFL.txt web/fonts/LBASystem8x8.ttf out/web/fonts/
ls -l out/web
