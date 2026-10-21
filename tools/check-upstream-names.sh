#!/usr/bin/env bash
# Fails when a tracked file names the project paw2-shell was forked from, outside the
# places that must keep it: the licence and credits, and code that reads data written
# before the split (the encrypted-file magic and the old Secret Service schema).
set -euo pipefail

cd "$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"

allowed=(
  ':!LICENSE'
  ':!CREDITS.md'
  ':!src/security/encrypted_file_store.h'
  ':!src/security/secret_store.h'
  ':!tests/secret_store_test.cpp'
  ':!tools/check-upstream-names.sh'
)

if matches=$(git grep -n -I -i "noctalia" -- . "${allowed[@]}"); then
  printf 'check-upstream-names: new mentions of the upstream project:\n%s\n' "$matches" >&2
  exit 1
fi
