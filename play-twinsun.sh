#!/bin/bash
# Play test: the Background row's Twinsun, the holomap's planet turning once in two minutes on stars.
#   ./play-twinsun.sh [--size 1920x1080]
# Writes .runs/twinsun/<time>/trace.log for reading back.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
size="1920x1080"
[[ "${1:-}" == "--size" ]] && size="$2"

cmake --build "$here/build" >/dev/null
run="$here/.runs/twinsun/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
# A copy, so a rebuild during the play test does not change what is played.
cp "$here/build/lba2menu" "$run/lba2menu"

cat <<'STEPS'
Steps (the window opens on Language and interface at 1920x1080, cursor on
Background, Twinsun picked):
  1. Watch it turn for a while. One turn in two minutes: right, too slow, too fast?
  2. Shift+7 steps the tilt (the note at the bottom right says "tstilt = N"):
     3328 looking up at the south pole (the start), then 0 edge on, 256 and 512
     from above, the north pole on top, 3840 a little below the equator, 3584.
  3. Shift+8 steps the size: 140 (the start), 80, 100 (the holomap's), 120.
  4. Walk a few pages (Esc, Display and graphics, Sound): do the rows read
     well over it? Esc on the hub closes the menu and shows it bare.
  5. Left and Right on Background: Picture, Blue, Twinsun, Off.
  6. F2 to 2560x1440 and back, and drag the window to an odd size.
  7. Close the window.
STEPS
read -r -p "Press Enter to open it. " _
"$run/lba2menu" --size "$size" --trace "$run/trace.log" \
    --do "open Language and interface; key down 3; set background 2" >"$run/out.log" 2>&1 || true
echo
echo "Trace: $run/trace.log"
grep -E " (switch|set|screen) " "$run/trace.log" || true
