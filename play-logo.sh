#!/bin/bash
# Play test: the intro's LBA 2 logo in the game menu's title place, drawn in code
# (switch videologo, F9; a trial on branch trial/video-logo).
#   ./play-logo.sh [--size 1920x1080]
# Writes .runs/logo/<time>/trace.log for reading back.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
size="1920x1080"
[[ "${1:-}" == "--size" ]] && size="$2"

cmake --build "$here/build" >/dev/null
run="$here/.runs/logo/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
cp "$here/build/lba2menu" "$run/lba2menu"

cat <<'STEPS'
Steps (the window opens on a fresh install's game menu, the logo building in):
  1. Watch it build: the logo fades in dark orange, a ring of water with blue beads
     crosses it from the upper right and bends the letters, it brightens, the 2 lands
     in a swirl and a second ripple runs through the letters. Then the cc arrives under
     the 2, one c after the other. About 3 seconds.
  2. F9 steps the switch; the toast says which. From 0 to 1 the build-in plays. From 1
     to 2 and 2 to 3 the build-in does NOT play: that step's ripple plays at once (about
     2.5 seconds), and again every 30 seconds.
       0  the wordmark as on main
       1  gentle: the letters sway as the ripple crosses, and stay in one piece
       2  as the intro: the letters bend hard, and bits of them break off for a moment
       3  as 2, with the arcs of blue beads sweeping across from the upper right
     To see one again, go round: F9 until the toast shows it once more.
  3. Shift+F9 turns the cc off and on without playing the build-in again (off is the
     logo as the intro has it).
     In the control file (its path is printed below), try the layout switch:
       echo "switch logosub 1" >> <control>     CLASSIC COMMUNITY kept under the logo
  4. Options, then Esc back: the logo should be still, not building in again.
  5. New game, then Esc: the game menu in play. The logo smaller, at a title's size,
     at rest: no build-in and no ripple there.
  6. F2 through the screen sizes; drag the window's edge. The logo should stay sharp.
  7. Language and interface > Font: the game font. The name falls back to one line.
  8. Close the window.
STEPS
echo "control: $run/control"
read -r -p "Press Enter to open it. " _
"$run/lba2menu" --first --size "$size" --trace "$run/trace.log" --control "$run/control" \
    --do "switch videologo 1" >"$run/out.log" 2>&1 || true
echo
echo "Trace: $run/trace.log"
grep -E "switch (videologo|logosub|logocc)| screen " "$run/trace.log" || true
