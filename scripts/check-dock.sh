#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
command -v qs >/dev/null 2>&1 || {
    echo "dock: quickshell (qs) not found, skipping dock-model test" >&2
    exit 0
}

box="$(mktemp -d)"
trap 'rm -rf "$box"' EXIT
mkdir -p "$box/cfg" "$box/home"
cp -a "$root/components" "$root/services" "$box/cfg/"
cp "$root/tests/dockmodel.qml" "$box/cfg/shell.qml"

out="$(env -u PAW2_SOCKET HOME="$box/home" XDG_CONFIG_HOME="$box/home/.config" \
    XDG_CACHE_HOME="$box/home/.cache" XDG_STATE_HOME="$box/home/.state" \
    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    timeout 90 qs -p "$box/cfg" 2>&1 | sed 's/\x1b\[[0-9;]*m//g')" || true

if grep -q "RESULT: all dock-model tests passed" <<<"$out"; then
    echo "dock: $(grep -c '  ok:' <<<"$out") dock-model assertions passed"
    exit 0
fi

echo "dock: dock-model test FAILED" >&2
grep -E "FAIL|RESULT|ERROR|caused by" <<<"$out" >&2 || echo "$out" >&2
exit 1
