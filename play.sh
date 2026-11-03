#!/bin/bash
# Play the prototype in a window on the maintainer's data (the US CD image).
#   ./play.sh [--size 1920x1080] [--first] [other lba2menu arguments]
# Writes .runs/play/<time>/: trace.log (every key and change), out.log (command
# replies) and control (append commands here to drive the open window).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
cmake --build "$here/build" >/dev/null
run="$here/.runs/play/$(date +%Y%m%d-%H%M%S)"
mkdir -p "$run"
ln -sfn "$run" "$here/.runs/play/latest"
echo "trace:   $run/trace.log"
echo "control: $run/control"
exec "$here/build/lba2menu" --trace "$run/trace.log" --control "$run/control" "$@" >"$run/out.log"
