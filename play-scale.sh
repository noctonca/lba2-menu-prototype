#!/bin/bash
# Play test: the menu at a proportional scale (switch propscale, F5; 5 at start).
#   ./play-scale.sh [--size 1440x900]
# Writes .runs/scale/<time>/trace.log for reading back.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
size="1440x900"
[[ "${1:-}" == "--size" ]] && size="$2"

cmake --build "$here/build" >/dev/null
run="$here/.runs/scale/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
cp "$here/build/lba2menu" "$run/lba2menu"

cat <<'STEPS'
Steps (the window opens on Display and graphics at 1440x900, game font, mode 5):
  1. F5 steps the mode, Shift+F5 back; the toast says which:
       0 today: whole step (1x here, the menu fills 60% of the height)
       1 everything linear at 1.5x, text too
       2 frames and plasma linear, text sharp, nearest at 1.5x
       3 frames linear, text sharp at 1x (smaller than its rows)
       4 frames linear, text sharp at 2x (bigger than its rows)
       5 frames linear, text: each font pixel its exact 1.5x square, edges blended
       6 as 5 after Scale2x, which rounds the font's curves and diagonals
  2. Which of 1, 2, 3, 5 and 6 reads best from where you sit? Is 0's smaller menu a problem?
  3. Walk Keys and buttons and Language and interface in your pick; watch the typed help.
  4. Language and interface > Font: Zilla Slab. It is drawn at 1.5x in every mode.
  5. F2 through the sizes (1280x720 is 1.33x, 2560x1440 2.67x); drag the window.
  6. Close the window.
STEPS
read -r -p "Press Enter to open it. " _
"$run/lba2menu" --size "$size" --trace "$run/trace.log" \
    --do "set font 0; open Display and graphics" >"$run/out.log" 2>&1 || true
echo
echo "Trace: $run/trace.log"
grep -E "switch propscale| set | screen " "$run/trace.log" || true
