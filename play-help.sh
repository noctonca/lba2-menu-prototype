#!/bin/bash
# Play test: the help in the band. Two rounds on the same steps, one window
# each; close the window to go on to the next round.
#   ./play-help.sh [--size 1920x1080]
# Round 1 is the help as first written; round 2 is as decided (2026-11-03), the
# default: one message at a time (a key clash's note alone, a dimmed row saying
# why) and the shorter help, so a translation half as long again still fits.
# Keys 7 and 8 turn each on and off in any round (F1 lists them).
# Writes .runs/help/<time>/round-<n>-<name>/trace.log for reading back.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
size="1920x1080"
[[ "${1:-}" == "--size" ]] && size="$2"

cmake --build "$here/build" >/dev/null
run="$here/.runs/help/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
# A copy, so a rebuild during the play test does not change what is played.
cp "$here/build/lba2menu" "$run/lba2menu"
# An unknown --switch is ignored, so look for it among the switches `state` lists.
if ! "$run/lba2menu" --headless --size "$size" --do "wait 1; state; quit" 2>/dev/null |
    grep '^state ' | grep -q '"short":'; then
    echo "This build has no 'short' switch yet." >&2
    exit 1
fi

cat <<'STEPS'
Steps, the same in each round (the window opens on the Options hub):
  1. Read the help of the Settings row, then Enter: read Modern and Classic.
     Down to Classic, Enter: read the question. Choose "No, keep Modern".
  2. Esc back. Down to Display and graphics, Enter. Read every row's help,
     Vsync, Detail level and Image above all. Do you miss what was cut?
  3. Esc back, Down to Controls and camera, Enter. Read Outdoors camera, then
     Invert camera and Camera speed (dimmed while Outdoors is Classic).
  4. Enter on Keys and buttons. Down to Actions and read its help. Enter: the
     cursor is on Behaviour action's Key 1. Enter, then press W (Talk and
     search's key). Read what the help says on the red key. Del clears it.
  5. Close the window.
STEPS

before=(--switch clash=0 --switch short=0)
round=1
for name in as-written decided; do
    dir="$run/round-$round-$name"
    mkdir -p "$dir"
    args=()
    [[ "$name" == as-written ]] && args=("${before[@]}")
    echo
    echo "Round $round: $name."
    read -r -p "Press Enter to open it. " _
    "$run/lba2menu" --size "$size" ${args[@]+"${args[@]}"} --trace "$dir/trace.log" >"$dir/out.log" 2>&1 || true
    round=$((round + 1))
done
echo
echo "Traces: $run"
