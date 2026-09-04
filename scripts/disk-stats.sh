#!/usr/bin/env bash
# Filesystem usage as JSON: { used, total, percent }  (GiB integers)
# Usage: disk-stats.sh [path]   # ponytail: default is the root filesystem
set -euo pipefail

path="${1:-/}"
# Fall back to / if the configured path doesn't exist (e.g. a typo'd mount).
[[ -e "$path" ]] || { echo "disk-stats: path '$path' not found, using /" >&2; path="/"; }

# timeout guards against an unresponsive mount hanging the widget
out=$(timeout 5 df -B1 --output=used,size,pcent "$path" 2>/dev/null | tail -1) || out=""
read -r used size pct <<< "$out"
pct=${pct%\%}; pct=${pct// /}
printf '{"used":%d,"total":%d,"percent":%d}\n' \
    "$(( ${used:-0} / 1073741824 ))" "$(( ${size:-0} / 1073741824 ))" "${pct:-0}"
