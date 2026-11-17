#!/bin/bash
# Play test: a game as lba2cc's goes, from a fresh install to Resume game.
#   ./play-flow.sh [--size 1920x1080]
# Writes .runs/flow/<time>/trace.log for reading back.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
size="1920x1080"
[[ "${1:-}" == "--size" ]] && size="$2"

cmake --build "$here/build" >/dev/null
run="$here/.runs/flow/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
cp "$here/build/lba2menu" "$run/lba2menu"

cat <<'STEPS'
Steps (the window opens as a fresh install: New game, Options, Quit, the logo building in):
  1. New game. The game is a landscape: Citadel Island, its lighthouse, clouds and waves moving.
  2. Enter a few times: each walks on to the next place (other hills). The third takes the
     boat to Desert Island; later the Emerald Moon at night, Otringal's towers, Celebration.
  3. Somewhere you like, Esc: the game menu in play (no picture there, as in lba2cc).
  4. Save game, New save: "Game saved." Then Resume game, walk on a place or two, Esc again.
  5. Quit: "The game would close here." Enter starts it again.
  6. The game menu now has Resume game with the picture of where step 4's Esc left you, and
     Load game. Load game: your save's picture is where you saved, AUTOSAVE's the last place.
  7. Resume game: back where Esc left you. Then close the window.
Watch for: does the order of things feel like starting lba2cc, playing, and coming back?
STEPS
read -r -p "Press Enter to open it. " _
"$run/lba2menu" --size "$size" --trace "$run/trace.log" >"$run/out.log" 2>&1 || true
echo
echo "Trace: $run/trace.log"
grep -E " (new game|scene |save |load |resume |open page=Game menu|quit chosen)" "$run/trace.log" || true
