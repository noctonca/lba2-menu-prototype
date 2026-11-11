#!/bin/bash
# Play test: the menu picture at the screen's own resolution (switch bgres).
#   ./play-bg.sh [--size 1920x1080]
# Writes .runs/bg/<time>/trace.log for reading back.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
size="1920x1080"
[[ "${1:-}" == "--size" ]] && size="$2"

cmake --build "$here/build" >/dev/null
run="$here/.runs/bg/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
# A copy, so a rebuild during the play test does not change what is played.
cp "$here/build/lba2menu" "$run/lba2menu"

cat <<'STEPS'
Steps (the window opens on Display and graphics at 1920x1080):
  1. Down to Image. Left and Right: Classic draws the picture in whole steps
     (crisp, the dither kept), Smooth draws it smooth. Look at the foam and at
     the picture under the darkened rows and the band.
  2. Drag the window's corner to odd sizes, as a browser window would be, with
     Image on Classic, then on Smooth. Classic crops more at some sizes (it
     takes the next whole factor up).
  3. Esc to the hub, Enter on Language and interface, Down to Background.
     Left and Right: Picture, Blue (palette 181, from the picture), Off. Does
     the cursor still stand out on Blue?
  4. Shift+3 still cycles the old fixed values for comparison (the note at the
     bottom right says "bgres = N"): 0 part of the canvas, 1 whole step,
     2 smooth, 3 as the Image row says.
  5. Close the window.
STEPS
read -r -p "Press Enter to open it. " _
"$run/lba2menu" --size "$size" --trace "$run/trace.log" \
    --do "open Display and graphics" >"$run/out.log" 2>&1 || true
echo
echo "Trace: $run/trace.log"
grep -E " (switch|screen) " "$run/trace.log" || true
