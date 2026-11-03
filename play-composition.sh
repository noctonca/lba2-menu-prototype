#!/bin/bash
# Play test: the wide composition on large screens. Two rounds on the same
# steps, one window each; close the window to go on to the next round.
#   ./play-composition.sh [--size 1920x1080]
# Both rounds show one pane at a time. Round 1 is the wide layout as it was
# before the review; round 2 is as decided (2026-11-03), the default: the
# 960x540 composition centred, its scale rule, the stacked legend, the picture
# scaled evenly and the band at level 15. Keys 1-6 turn each part on and off in any
# round (F1 lists them), and F2 keeps them while it steps through the sizes.
# Writes .runs/composition/<time>/round-<n>-<name>/trace.log for reading back.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
size="1920x1080"
[[ "${1:-}" == "--size" ]] && size="$2"

cmake --build "$here/build" >/dev/null
run="$here/.runs/composition/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
# A copy, so a rebuild during the play test does not change what is played.
cp "$here/build/lba2menu" "$run/lba2menu"
# An unknown --switch is ignored, so look for it among the switches `state` lists.
if ! "$run/lba2menu" --headless --size "$size" --do "wait 1; state; quit" 2>/dev/null |
    grep '^state ' | grep -q '"panes":'; then
    echo "This build has no 'panes' switch yet." >&2
    exit 1
fi

cat <<'EOF'
Steps, the same in each round (the window opens on Display and graphics):
  1. Before pressing anything: where does your eye go first? Read the help in
     the band from where you would sit to play.
  2. Down through the rows to Image, then Esc back to the categories, Down to
     Controls and camera, Enter. Read Outdoors camera and its value.
  3. Enter on Keys and buttons, Right onto Forward's Key 1. Read the help.
     Tab to the Gamepad view and back.
  4. F2 through the screen sizes (1280x800, 1920x1080, 2400x1080, 2560x1440,
     3440x1440, 3840x2160). At each: is the menu where you expect it? Is the
     help readable over the picture?
  5. Close the window.
EOF

before=(--switch scale=0 --switch sheet=0 --switch legend=0 --switch bg=0 --switch bandshade=0)
round=1
for name in as-it-was decided; do
    dir="$run/round-$round-$name"
    mkdir -p "$dir"
    args=()
    [[ "$name" == as-it-was ]] && args=("${before[@]}")
    echo
    echo "Round $round: $name."
    read -r -p "Press Enter to open it. " _
    "$run/lba2menu" --size "$size" ${args[@]+"${args[@]}"} --trace "$dir/trace.log" \
        --do "open Display and graphics" >"$dir/out.log" 2>&1 || true
    round=$((round + 1))
done
echo
echo "Traces: $run"
