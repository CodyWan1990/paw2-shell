#!/usr/bin/env bash
# Delete a single entry from cliphist history.
# Usage: clip-delete.sh "<cliphist list line>"
set -euo pipefail
printf '%s' "${1:?usage: clip-delete.sh <line>}" | cliphist delete
