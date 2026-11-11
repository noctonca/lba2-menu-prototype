#!/bin/bash
# Play test: the menu's letters in the game font or Zilla Slab (the Font row).
#   ./play-font.sh [--size 1920x1080]
# Writes .runs/font/<time>/trace.log for reading back.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
size="1920x1080"
[[ "${1:-}" == "--size" ]] && size="$2"

cmake --build "$here/build" >/dev/null
run="$here/.runs/font/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
# A copy, so a rebuild during the play test does not change what is played.
cp "$here/build/lba2menu" "$run/lba2menu"

cat <<'STEPS'
Steps (the window opens on Language and interface at 1920x1080, cursor on Font):
  1. Right and Left: Game, Zilla Slab (Bold). The whole menu changes, the help
     in the band too; the small key chips keep their 8x8 letters.
  2. Does it read well beside the 1997 frames and plasma?
  3. Shift+9 and Shift+0 make Zilla 5% smaller or larger (the note at the bottom
     right says "vfsize = N"; 100 = capitals as tall as the game font's).
     Find the size that looks right, or say it already is.
  4. Walk a few pages with Zilla on: Display, Keys and buttons (Enter on a
     group), Advanced and reset. Watch the typed help.
  5. F2 to 2560x1440 and back, and drag the window to an odd size: the letters
     stay sharp at every size.
  6. Close the window.
STEPS
read -r -p "Press Enter to open it. " _
"$run/lba2menu" --size "$size" --trace "$run/trace.log" \
    --do "open Language and interface; key down 4" >"$run/out.log" 2>&1 || true
echo
echo "Trace: $run/trace.log"
grep -E " (switch|set|screen) |font" "$run/trace.log" || true
