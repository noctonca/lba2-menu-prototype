#!/bin/bash
# Play test: the help window's legend at the size that fits (switch legendfit, F8; off at start).
#   ./play-legendfit.sh [--size 1024x768]
# Writes .runs/legendfit/<time>/trace.log for reading back.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
size="1024x768"
[[ "${1:-}" == "--size" ]] && size="$2"

cmake --build "$here/build" >/dev/null
run="$here/.runs/legendfit/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
cp "$here/build/lba2menu" "$run/lba2menu"

cat <<'STEPS'
Steps (the window opens on Keys and buttons at 1024x768, the one column, game font):
  1. Look at the legend under the help: Enter Open, Del Defaults, Tab Gamepad, Esc Back.
     It does not fit at 2x, so today it drops to 1x.
  2. F8 flips legendfit; the toast says which. 1: the row at the size that fills the
     window's width, between 1x and 2x. Flip a few times from where you sit.
  3. Move the mouse over the legend: each entry lights where it is drawn; a click on
     Defaults asks (Esc cancels).
  4. Language and interface > Text: Deutsch, then back to Keys and buttons: the
     German words are longer, so the row is smaller (under 1.5x the chips keep the
     1x corners).
  5. F2 through the sizes (640x480 is the 1997 screen; at 1280x960 and up the row fits
     at 2x and F8 changes nothing).
  6. Close the window.
STEPS
read -r -p "Press Enter to open it. " _
"$run/lba2menu" --size "$size" --trace "$run/trace.log" \
    --do "set font 0; open Keys and buttons" >"$run/out.log" 2>&1 || true
echo
echo "Trace: $run/trace.log"
grep -E "switch legendfit| set | screen " "$run/trace.log" || true
