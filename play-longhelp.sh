#!/bin/bash
# Play test: long help. A row's short line, then a second window on Space (the pad's Y)
# saying when or why to change it; "More" in the legend, "..." where a window goes on.
#   ./play-longhelp.sh [--size 1920x1080]
# Writes .runs/longhelp/<time>/trace.log for reading back.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
size="1920x1080"
[[ "${1:-}" == "--size" ]] && size="$2"

cmake --build "$here/build" >/dev/null
run="$here/.runs/longhelp/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
# A copy, so a rebuild during the play test does not change what is played.
cp "$here/build/lba2menu" "$run/lba2menu"

cat <<'STEPS'
Steps (the window opens on Sound, cursor on Master volume):
  1. Read the short line (it ends in "..."), then press Space (or Y on a pad):
     the second window is typed, starting with "...". Space again goes back.
  2. Walk down the rows. Each move starts at the short line again. Is the
     second window worth a key press? Is the short line still enough while
     scrolling?
  3. Esc, then Language and interface, Display, Controls and camera: the rows
     with a second window show More in the legend; the rest do not.
  4. Keys and buttons: in Movement, Back has a second window; in Actions, so
     do Behavior action and Behavior menu. Change a key and press Space while
     it waits: Space is taken as the key, not as More.
  5. F4 turns long help off (the short help alone, as before) and on again.
  6. Text to Deutsch (Language and interface), then read a few second windows.
  7. F2 down to 640x480: the same in the dialogue window.
  8. Close the window.
STEPS
read -r -p "Press Enter to open it. " _
"$run/lba2menu" --size "$size" --trace "$run/trace.log" \
    --do "open Sound" >"$run/out.log" 2>&1 || true
echo
echo "Trace: $run/trace.log"
grep -E "help window|switch longhelp|open page|screen " "$run/trace.log" || true
