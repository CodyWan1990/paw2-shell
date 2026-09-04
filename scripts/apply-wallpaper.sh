#!/usr/bin/env bash
# Set the wallpaper with swaybg and cache the path to ~/.cache/current-wallpaper.
#
# The cache file is a record, not a restore mechanism: nothing reads it. swaybg
# dies with the session, and services/Theme.qml re-runs this script once at
# startup for the saved theme (see restoreWallpaper) - that is what puts the
# wallpaper back at login.
#
# Usage: apply-wallpaper.sh <image-path> [mode]
set -euo pipefail

img="${1:?usage: apply-wallpaper.sh <image-path> [mode]}"
mode="${2:-fill}"

[[ -f "$img" ]] || { echo "apply-wallpaper: not found: $img" >&2; exit 1; }

printf '%s' "$img" > "$HOME/.cache/current-wallpaper"

# Replace any running swaybg, then start a fresh one detached.
pkill -x swaybg 2>/dev/null || true
swaybg -i "$img" -m "$mode" >/dev/null 2>&1 &
disown
echo "wallpaper set: $img ($mode)"
