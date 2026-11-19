#!/bin/bash
# Play test: Resume game's opening (switch opening, Shift+F10): the logo builds in where
# the save's picture goes, rests, then cross-fades into the picture.
#   ./play-opening.sh [--size 1920x1080]
# Writes .runs/opening/<time>/trace.log for reading back.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
size="1920x1080"
[[ "${1:-}" == "--size" ]] && size="$2"

cmake --build "$here/build" >/dev/null
run="$here/.runs/opening/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
cp "$here/build/lba2menu" "$run/lba2menu"

cat <<'STEPS'
Steps (the window opens on a returning player's game menu, the cursor on Resume game):
  1. Watch: the logo builds in where the picture goes (about 3 s, the cc last), rests
     about 1.5 s, then fades into the save's picture (half a second). Is the rest long
     enough? Is the fade too quick or too slow?
  2. Shift+F10 twice: off (the picture at once, as before), then on (the opening again).
  3. Shift+F10 twice again, and press Down while the logo is still building: it should
     keep building. Up: the picture, at once.
  4. Shift+F10 twice again, and press Enter while the logo builds: the game starts at
     once. Esc back to the game menu.
  5. F2 to 640x480, Shift+F10 twice: the same, smaller.
  6. Close the window.
STEPS
read -r -p "Press Enter to open it. " _
"$run/lba2menu" --saves 20 --size "$size" --trace "$run/trace.log" >"$run/out.log" 2>&1 || true
echo
echo "Trace: $run/trace.log"
grep -iE "switch opening| screen |resume" "$run/trace.log" || true
