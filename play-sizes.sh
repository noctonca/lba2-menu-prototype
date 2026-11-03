#!/bin/bash
# Play test: the menu at every screen size, switched in place. One window; the
# page, the cursor and the settings stay as the size changes.
#   ./play-sizes.sh [--size 640x480]
# Writes .runs/sizes/<time>/trace.log for reading back (each change is a
# "screen" line, and a "layout" line when it crosses one column and wide).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
size="640x480"
[[ "${1:-}" == "--size" ]] && size="$2"

cmake --build "$here/build" >/dev/null
run="$here/.runs/sizes/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
# A copy, so a rebuild during the play test does not change what is played.
cp "$here/build/lba2menu" "$run/lba2menu"

cat <<'EOF'
Steps (the window opens on Display and graphics, one column):
  1. Down to Vsync. Press F2 through the sizes. At each, is the cursor still
     on Vsync, and does the page fit? 1280x720 is the first wide one.
  2. Shift+F2 back down to 640x480, still on Vsync.
  3. Change a value (Left or Right on Vsync), then F2 again: the value stays.
  4. Up to Resolution. Right and Left change the screen as F2 does, and the
     row says which one you are on. Does it read as a setting that took?
  5. Drag the window's corner slowly, wider and narrower, taller and shorter.
     The layout follows; where does it turn from one column to wide and back?
     Resolution then shows the dragged size.
  6. Esc to the hub, Down to Controls and camera, Enter on Keys and buttons,
     Down to Behaviours. F2 and drag again: still on Behaviours?
  7. Close the window.
EOF
read -r -p "Press Enter to open it. " _
"$run/lba2menu" --size "$size" --trace "$run/trace.log" \
    --do "open Display and graphics" >"$run/out.log" 2>&1 || true
echo
echo "Trace: $run/trace.log"
grep -E " (screen|layout) " "$run/trace.log" || true
