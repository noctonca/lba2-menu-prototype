#!/usr/bin/env bash
# The screen size changed in place (F2, a dragged window, the `size` command)
# must leave the menu as a fresh start at the new size would show it on the same
# row: every box, the cursor, the help. Compares `state` both ways for pages x
# start sizes x target sizes, and F2 stepping through the list and back round.
#
#   tools/resize-check.sh [BIN]   (default: build-test, built first by tools/test_build.py)
set -u
BIN=${1:-$(python3 "$(dirname "$0")/test_build.py")} || exit 2
PAGES=("Display and graphics" "Sound" "Controls and camera" "Advanced and reset" "original"
       "Keys and buttons" "More options")
STARTS=(1280x720 640x480 3840x2160 1280x800)
TARGETS=(1920x1080 768x480 1024x480 640x480 3440x1440 1280x800 2400x1080 3840x2160 1512x904)

state() { "$BIN" --headless --switch whole=1 "$@" 2>/dev/null | grep '^state ' | sed -E 's/"t":[0-9]+,//'; }

runs=0 bad=0
check() {  # name, extra switches, page, start, target, downs for the fresh run
    local inplace fresh
    inplace=$(state --size "$4" $2 --do "open $3; key down; key down; wait 3; size $5; wait 3; state")
    # Launched at the start size too: a headless run's native size is its launch size,
    # and the Resolution row marks it "(native)". The size changes before the page opens.
    fresh=$(state --size "$4" $2 --do "size $5; wait 3; open $3; key down $6; wait 6; state")
    runs=$((runs + 1))
    if [ -z "$inplace" ] || [ "$inplace" != "$fresh" ]; then
        echo "DIFF $1: $3, $4 -> $5"
        bad=$((bad + 1))
    fi
}

for page in "${PAGES[@]}"; do
    for start in "${STARTS[@]}"; do
        for target in "${TARGETS[@]}"; do
            # No layout has a Return row, so the cursor is on the same row both ways.
            check pages "" "$page" "$start" "$target" 2
        done
    done
done
# F2 once for each size comes back round to where it started, on the same row.
a=$(state --size 640x480 --do "open Sound; key down; key down; wait 2; state")
b=$(state --size 640x480 --do "open Sound; key down; key down; wait 2; key f2 10; wait 2; state" | sed -n 1p)
runs=$((runs + 1))
[ -n "$a" ] && [ "$(echo "$a" | grep -o '"cursor_row":"[^"]*"')" = "$(echo "$b" | grep -o '"cursor_row":"[^"]*"')" ] &&
    echo "$b" | grep -q '"physical":\[640,480\]' || { echo "DIFF F2 round trip"; bad=$((bad + 1)); }

echo "resize-check: $runs runs, $bad differ"
[ "$bad" -eq 0 ]
