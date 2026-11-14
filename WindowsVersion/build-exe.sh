#!/usr/bin/env bash
# Build the standalone Windows executable for The Last Journey.
#
#   cd WindowsVersion
#   ./build-exe.sh
#
# Output: WindowsVersion/TheLastJourney.exe (single file, game embedded).
# Needs Node (npx) and network access the first time, to fetch the
# Neutralino framework binaries. Nothing else — no Android SDK, no Java.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"

printf '==> Bundling game sources\n'
( cd "$ROOT" && npx --yes esbuild src/main.js --bundle --format=iife \
    --target=es2020 --charset=utf8 --log-level=warning \
    --outfile="$HERE/resources/game.bundle.js" )
cp "$ROOT/styles.css" "$HERE/resources/styles.css"

if [ ! -f "$HERE/bin/neutralino-win_x64.exe" ]; then
  printf '==> Fetching Neutralino framework\n'
  ( cd "$HERE" && npx --yes @neutralinojs/neu update )
fi

printf '==> Building single-file executable\n'
( cd "$HERE" && npx --yes @neutralinojs/neu build --embed-resources )

cp "$HERE/dist/TheLastJourney/TheLastJourney-win_x64.exe" "$HERE/TheLastJourney.exe"

printf '\nWrote %s\n' "$HERE/TheLastJourney.exe"
if command -v sha256sum >/dev/null 2>&1; then
  sha256sum "$HERE/TheLastJourney.exe" | sed 's/^/  sha256    /'
fi
