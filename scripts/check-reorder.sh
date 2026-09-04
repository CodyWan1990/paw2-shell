#!/usr/bin/env bash
# Run tests/barreorder.qml — the BarTab widget-reorder regression test.
#
# Same staging trick as check-nav.sh: Quickshell will not import a module from
# outside its config folder, so the test is copied in beside components/,
# services/ and panels/ rather than importing upwards. HOME is redirected
# because Settings writes a real dotfile.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"

command -v qs >/dev/null 2>&1 || {
    echo "reorder: quickshell (qs) not found, skipping bar-reorder test" >&2
    exit 0
}

box="$(mktemp -d)"
trap 'rm -rf "$box"' EXIT
mkdir -p "$box/cfg" "$box/home"
cp -a "$root/components" "$root/services" "$root/panels" "$box/cfg/"
cp "$root/tests/barreorder.qml" "$box/cfg/shell.qml"

# Qt.quit() does not stop Quickshell, so the run is bounded and judged by what
# it printed rather than by its exit status.
out="$(HOME="$box/home" XDG_CONFIG_HOME="$box/home/.config" \
    XDG_CACHE_HOME="$box/home/.cache" XDG_STATE_HOME="$box/home/.state" \
    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    timeout 90 qs -p "$box/cfg" 2>&1 | sed 's/\x1b\[[0-9;]*m//g')" || true

if grep -q "RESULT: all bar-reorder tests passed" <<<"$out"; then
    echo "reorder: $(grep -c '  ok:' <<<"$out") bar-reorder assertions passed"
    exit 0
fi

echo "reorder: bar-reorder test FAILED" >&2
grep -E "FAIL|RESULT|ERROR|caused by" <<<"$out" >&2 || echo "$out" >&2
exit 1
