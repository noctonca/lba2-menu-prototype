#!/bin/bash
# Play test: the Controls column heads above the rows (heads=0) or inside them
# (heads=1). Two rounds on the same steps, one window each; close the window
# (or Esc out of the Options) to go on to the next round.
#   ./play-heads.sh [--size 1920x1080]
# Writes .runs/heads/<time>/round-<n>-heads-<v>/trace.log for reading back.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
size="1920x1080"
[[ "${1:-}" == "--size" ]] && size="$2"

cmake --build "$here/build" >/dev/null
run="$here/.runs/heads/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
# A copy, so a rebuild during the play test does not change what is played.
cp "$here/build/lba2menu" "$run/lba2menu"
# An unknown --switch is ignored, so look for it among the switches `state` lists.
if ! "$run/lba2menu" --headless --size "$size" --do "wait 1; state; quit" 2>/dev/null |
    grep '^state ' | grep -q '"heads":'; then
    echo "This build has no 'heads' switch yet." >&2
    exit 1
fi

cat <<'EOF'
Steps, the same in each round (the window opens on Keys and buttons, Movement):
  1. Look at the page before pressing anything: which device is shown, and
     which column is Key 1 and which Key 2?
  2. Right: the cursor goes on Forward's Key 1. Read the column name for it.
  3. Esc, then Down 3 to Weapons, Right, then Down until Weapon 7 (the last).
     Does the page scroll? Do the column names stay readable?
  4. Tab to the Gamepad view and back. Is it clear which device you are on?
  5. F2 a few times to try other screen sizes, back to the start.
  6. Close the window.
EOF

round=1
for v in 1 0; do
    dir="$run/round-$round-heads-$v"
    mkdir -p "$dir"
    echo
    echo "Round $round: heads=$v ($([[ $v == 1 ]] && echo 'inside the rows' || echo 'a head line above'))."
    read -r -p "Press Enter to open it. " _
    "$run/lba2menu" --size "$size" --switch heads=$v --trace "$dir/trace.log" \
        --do "open Keys and buttons" >"$dir/out.log" 2>&1 || true
    round=$((round + 1))
done
echo
echo "Traces: $run"
