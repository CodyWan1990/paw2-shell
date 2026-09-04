#!/usr/bin/env bash
# Run tests/navcursor.qml — the settings row-cursor regression test.
#
# Quickshell will not import a module from outside its config folder, so the
# test cannot sit next to components/ and import upwards; it gets staged into a
# throwaway config with copies instead. HOME is redirected too, because the
# services read and write real dotfiles.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"

command -v qs >/dev/null 2>&1 || {
    echo "nav: quickshell (qs) not found, skipping row-cursor test" >&2
    exit 0
}

box="$(mktemp -d)"
trap 'rm -rf "$box"' EXIT
mkdir -p "$box/cfg" "$box/home"
cp -a "$root/components" "$root/services" "$box/cfg/"
cp "$root/tests/navcursor.qml" "$box/cfg/shell.qml"

# Qt.quit() does not stop Quickshell, so the run is bounded and judged by what
# it printed rather than by its exit status.
out="$(HOME="$box/home" XDG_CONFIG_HOME="$box/home/.config" \
    XDG_CACHE_HOME="$box/home/.cache" XDG_STATE_HOME="$box/home/.state" \
    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    timeout 90 qs -p "$box/cfg" 2>&1 | sed 's/\x1b\[[0-9;]*m//g')" || true

if grep -q "RESULT: all row-cursor tests passed" <<<"$out"; then
    echo "nav: $(grep -c '  ok:' <<<"$out") row-cursor assertions passed"
    exit 0
fi

echo "nav: row-cursor test FAILED" >&2
grep -E "FAIL|RESULT|ERROR|caused by" <<<"$out" >&2 || echo "$out" >&2
exit 1
