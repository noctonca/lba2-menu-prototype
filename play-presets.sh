#!/bin/bash
# Play test: presets that set the look (switch presetlook): Classic the 1997 menu at
# 640 x 480, Modern Twinsun, Zilla Slab, the Auto cameras and the starting screen.
#   ./play-presets.sh [--size 1920x1080] [--first]
# Writes .runs/presets/<time>/trace.log for reading back.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
size="1920x1080"
first=()
while [[ $# -gt 0 ]]; do
    case "$1" in
    --size) size="$2"; shift 2 ;;
    --first) first=(--first); shift ;;
    *) echo "unknown argument $1" >&2; exit 2 ;;
    esac
done

cmake --build "$here/build" >/dev/null
run="$here/.runs/presets/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
# A copy, so a rebuild during the play test does not change what is played.
cp "$here/build/lba2menu" "$run/lba2menu"

cat <<'STEPS'
Steps (the window opens on Options at 1920x1080 with Modern: Twinsun, Zilla Slab):
  1. Enter on Preset, then Classic, and Yes. The menu should turn into the 1997
     one: the picture, the game font, 640 x 480, one column.
  2. Walk a few pages (Language and interface, Controls and camera): the rows
     read Classic. Then back to Preset and Modern: everything comes back.
  3. Classic again, then Undo preset: the screen and the look come back.
  4. Advanced and reset > Reset settings from Classic: Modern's look; Undo reset.
  5. Shift+1 turns the switch off (presets set the rules only, as the plan has
     it) and back on. Which reads better as a demo?
  6. Close the window. (--first starts on the first-launch choice.)
STEPS
read -r -p "Press Enter to open it. " _
"$run/lba2menu" --size "$size" "${first[@]}" --trace "$run/trace.log" >"$run/out.log" 2>&1 || true
echo
echo "Trace: $run/trace.log"
grep -E " (switch|set|screen|apply|reset|back to) " "$run/trace.log" || true
