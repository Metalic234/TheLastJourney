#!/usr/bin/env bash
# Build and run the pocket edition on the host (no display needed).
#   ./build-host.sh            compile, run, dump frames to ./frames
#   ./build-host.sh --no-run   compile only
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${TMPDIR:-/tmp}/tlj_pocket"
mkdir -p "$HERE/frames"

cc -O2 -std=c99 -Wall -Wextra -DGAME_TEST -I"$HERE/TheLastJourney" \
  "$HERE/host/main_host.c" "$HERE/TheLastJourney/game.c" \
  -lm -o "$OUT"

echo "built $OUT"

if [ "${1:-}" != "--no-run" ]; then
  "$OUT" "$HERE/frames"
  echo "frames written to $HERE/frames"
fi
