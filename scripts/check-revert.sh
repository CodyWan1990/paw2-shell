#!/usr/bin/env bash
# Run tests/dropinrevert.qml — PawConfig's table-claiming and refused-write revert.
#
# Same staging trick as check-snapshot.sh. PAW2_SOCKET is unset on purpose: with
# it set, the Paw singleton connects to the running compositor and real config
# messages land in the middle of the assertions.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"

command -v qs >/dev/null 2>&1 || {
    echo "revert: quickshell (qs) not found, skipping drop-in revert test" >&2
    exit 0
}

box="$(mktemp -d)"
trap 'rm -rf "$box"' EXIT
mkdir -p "$box/cfg" "$box/home"
cp -a "$root/components" "$root/services" "$box/cfg/"
cp "$root/tests/dropinrevert.qml" "$box/cfg/shell.qml"

# Qt.quit() does not stop Quickshell, so the run is bounded and judged by what
# it printed rather than by its exit status.
out="$(env -u PAW2_SOCKET HOME="$box/home" XDG_CONFIG_HOME="$box/home/.config" \
    XDG_CACHE_HOME="$box/home/.cache" XDG_STATE_HOME="$box/home/.state" \
    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    timeout 90 qs -p "$box/cfg" 2>&1 | sed 's/\x1b\[[0-9;]*m//g')" || true

if grep -q "RESULT: all drop-in revert tests passed" <<<"$out"; then
    echo "revert: $(grep -c '  ok:' <<<"$out") drop-in revert assertions passed"
    exit 0
fi

echo "revert: drop-in revert test FAILED" >&2
grep -E "FAIL|RESULT|ERROR|caused by" <<<"$out" >&2 || echo "$out" >&2
exit 1
