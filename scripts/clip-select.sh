#!/usr/bin/env bash
# Decode a cliphist history line and copy it to the clipboard.
# Usage: clip-select.sh "<cliphist list line>"
set -euo pipefail
printf '%s' "${1:?usage: clip-select.sh <line>}" | cliphist decode | wl-copy
