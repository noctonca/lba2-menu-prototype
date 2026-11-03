#!/bin/bash
# Play test: the Experimental settings page, as a dense table and as a list.
# Two rounds per size on the same steps, one window each; close the window to
# go on to the next round.
#   ./play-experimental.sh                      1920x1080, then 640x480
#   ./play-experimental.sh --size 2560x1440     one size only
# Round 1 is the dense table in the 8x8 font (design card ExperimentalTable, the
# plan's choice, the default); round 2 the game-font list, a page per group
# (ExperimentalList). Key 0 switches between them in any round, F5 the 8x8 font
# between 2x and 1x (F1 lists the keys).
# Writes .runs/experimental/<time>/round-<n>-<name>/trace.log for reading back.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
sizes=(1920x1080 640x480)
[[ "${1:-}" == "--size" ]] && sizes=("$2")
size="${sizes[0]}"

cmake --build "$here/build" >/dev/null
run="$here/.runs/experimental/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
# A copy, so a rebuild during the play test does not change what is played.
cp "$here/build/lba2menu" "$run/lba2menu"
# An unknown --switch is ignored, so look for it among the switches `state` lists.
if ! "$run/lba2menu" --headless --size "$size" --do "wait 1; state; quit" 2>/dev/null |
    grep '^state ' | grep -q '"exptable":'; then
    echo "This build has no 'exptable' switch yet." >&2
    exit 1
fi

cat <<'STEPS'
Steps, the same in each round (the window opens on Options):
  1. Down to Advanced and reset, Enter. Down to Experimental settings,
     read its help, Enter.
  2. Find cam_ease_ms (Auto camera) and turn it up to 450 with Right.
     Find cam_glide (Mouse) and turn it down to 60. In the table, try
     Page Down and Page Up (the pad's LB and RB) on the way.
  3. Read the help of a few rows. Would you know what each does?
  4. On cam_glide, Del: back to 75. Esc out to the Options.
  5. Read the Settings row: it says "changed". Enter, then Enter on Modern:
     the tuning goes back to the defaults. Down to Back to my settings and
     read what it names; Enter puts cam_ease_ms back.
  6. Close the window.
STEPS

round=1
for size in "${sizes[@]}"; do
    for name in table list; do
        dir="$run/round-$round-$size-$name"
        mkdir -p "$dir"
        form=1
        [[ "$name" == list ]] && form=0
        echo
        echo "Round $round: $size, $name."
        read -r -p "Press Enter to open it. " _
        "$run/lba2menu" --size "$size" --switch exptable=$form --trace "$dir/trace.log" >"$dir/out.log" 2>&1 || true
        round=$((round + 1))
    done
done
echo
echo "Traces: $run"
