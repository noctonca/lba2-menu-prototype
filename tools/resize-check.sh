#!/usr/bin/env bash
# The screen size changed in place (F2, a dragged window, the `size` command)
# must leave the menu as a fresh start at the new size would show it on the same
# row: every box, the cursor, the help. Compares `state` both ways for pages x
# start sizes x target sizes, and F2 stepping through the list and back round.
#
#   tools/resize-check.sh [build/lba2menu]
set -u
BIN=${1:-build/lba2menu}
PAGES=("Display and graphics" "Sound" "Controls and camera" "Advanced and reset" "original"
       "Keys and buttons" "Experimental settings")
STARTS=(1280x720 640x480 3840x2160 1280x800)
TARGETS=(1920x1080 640x480 3440x1440 1280x800 2400x1080 3840x2160 1512x904)

state() { "$BIN" --headless --switch typing=2 "$@" 2>/dev/null | grep '^state ' | sed -E 's/"t":[0-9]+,//'; }
column() { "$BIN" --headless --size "$1" --do state 2>/dev/null | grep -q '"layout":"column"' && echo 1 || echo 0; }

declare -A COL
for s in "${STARTS[@]}" "${TARGETS[@]}"; do COL[$s]=$(column "$s"); done

runs=0 bad=0
check() {  # name, extra switches, page, start, target, downs for the fresh run
    local inplace fresh
    inplace=$(state --size "$4" $2 --do "open $3; key down; key down; wait 3; size $5; wait 3; state")
    fresh=$(state --size "$5" $2 --do "open $3; key down $6; wait 6; state")
    runs=$((runs + 1))
    if [ -z "$inplace" ] || [ "$inplace" != "$fresh" ]; then
        echo "DIFF $1: $3, $4 -> $5"
        bad=$((bad + 1))
    fi
}

for page in "${PAGES[@]}"; do
    for start in "${STARTS[@]}"; do
        for target in "${TARGETS[@]}"; do
            # A page's one-column cursor counts its Return row; wide's does not.
            # Controls and Experimental keep their own cursors, without it.
            case "$page" in
            "Keys and buttons" | "Experimental settings") downs=2 ;;
            *) downs=$((2 - COL[$start] + COL[$target])) ;;
            esac
            check pages "" "$page" "$start" "$target" "$downs"
        done
    done
done
# The Experimental page as a list of groups, as switch 0 shows it.
for start in "${STARTS[@]}"; do
    for target in "${TARGETS[@]}"; do
        check exptable=0 "--switch exptable=0" "Experimental settings" "$start" "$target" 2
    done
done

# F2 eight times comes back round to where it started, on the same row.
a=$(state --size 640x480 --do "open Sound; key down; key down; wait 2; state")
b=$(state --size 640x480 --do "open Sound; key down; key down; wait 2; key f2 8; wait 2; state" | sed -n 1p)
runs=$((runs + 1))
[ -n "$a" ] && [ "$(echo "$a" | grep -o '"cursor_row":"[^"]*"')" = "$(echo "$b" | grep -o '"cursor_row":"[^"]*"')" ] &&
    echo "$b" | grep -q '"physical":\[640,480\]' || { echo "DIFF F2 round trip"; bad=$((bad + 1)); }

echo "resize-check: $runs runs, $bad differ"
[ "$bad" -eq 0 ]
