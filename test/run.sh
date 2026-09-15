#!/bin/bash
# Drive the tetris ROM through a command script in headless Mesen and collect
# screenshots. Usage: test/run.sh [commands-file]
# Screenshots land in /tmp/mesen_harness/*.png
set -e
shopt -s nullglob

HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$(dirname "$HERE")"
ROM="$PROJ/tetris.sfc"
LUA="$HERE/harness.lua"
MESEN="/Applications/Mesen.app/Contents/MacOS/Mesen"
OUT="/tmp/mesen_harness"

CMDS="${1:-$HERE/tests/phase2.txt}"

[ -f "$ROM" ] || { echo "ROM not found: $ROM (run make first)"; exit 1; }
[ -f "$CMDS" ] || { echo "commands file not found: $CMDS"; exit 1; }

mkdir -p "$OUT"
for f in "$OUT"/*.png; do rm -f "$f"; done
cp "$CMDS" "$OUT/commands.txt"

pkill -9 -f Mesen.app 2>/dev/null || true
sleep 1

echo "Running headless: $(basename "$CMDS")"
"$MESEN" --testrunner "$ROM" "$LUA" 2>&1 | tail -5 || true
echo "--- screenshots ---"
shots=("$OUT"/*.png)
if [ ${#shots[@]} -eq 0 ]; then echo "(none produced)"; else printf '%s\n' "${shots[@]}"; fi
