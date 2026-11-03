#!/bin/bash
# Play test: the Frame rate row, the Console row with the version, and the console.
# Two rounds per size on the same steps, one window each; close the window to
# go on to the next round.
#   ./play-console.sh                      1920x1080, then 640x480
#   ./play-console.sh --size 2560x1440     one size only
# Round 1 puts Frame rate on Display and graphics, under Vsync; round 2 puts it
# on Advanced and reset (switch fpsrow, no key). The Console row is on Advanced
# and reset in both, with the version as its value.
# Writes .runs/console/<time>/round-<n>-<name>/trace.log for reading back.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
sizes=(1920x1080 640x480)
[[ "${1:-}" == "--size" ]] && sizes=("$2")
size="${sizes[0]}"

cmake --build "$here/build" >/dev/null
run="$here/.runs/console/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
# A copy, so a rebuild during the play test does not change what is played.
cp "$here/build/lba2menu" "$run/lba2menu"
# An unknown --switch is ignored, so look for it among the switches `state` lists.
if ! "$run/lba2menu" --headless --size "$size" --do "wait 1; state; quit" 2>/dev/null |
    grep '^state ' | grep -q '"fpsrow":'; then
    echo "This build has no 'fpsrow' switch yet." >&2
    exit 1
fi

cat <<'STEPS'
Steps, the same in each round (the window opens on Options):
  1. Find Frame rate (round 1: Display and graphics; round 2: Advanced and
     reset). Turn it on: the count shows in the bottom left corner. (In the
     game it shows only while you play; here it shows over the menu.) Look at
     the page: in round 1 at 1920x1080 it now scrolls, Image is below.
  2. Go to Advanced and reset. Read the Console row: the version is its
     value. Would you find it there when asked "which build"?
  3. With the keyboard, Enter on Console: the full console, top half, with a
     prompt. Down and Enter do not close it; Esc does (F12 in the game; F12
     is a prototype switch here).
  4. With a pad, if one is connected: A on Console opens it read-only, with
     "Read only. Any button closes it." as its last line. Press any button.
  5. Close the window.
STEPS

round=1
for size in "${sizes[@]}"; do
    for name in display advanced; do
        dir="$run/round-$round-$size-fps-on-$name"
        mkdir -p "$dir"
        row=0
        [[ "$name" == advanced ]] && row=1
        echo
        echo "Round $round: $size, Frame rate on $name."
        read -r -p "Press Enter to open it. " _
        "$run/lba2menu" --size "$size" --switch fpsrow=$row --trace "$dir/trace.log" >"$dir/out.log" 2>&1 || true
        round=$((round + 1))
    done
done
echo
echo "Traces: $run"
