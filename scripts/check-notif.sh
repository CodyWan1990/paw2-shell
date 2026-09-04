#!/usr/bin/env bash
# Run tests/notifhistory.qml — notification history/toast-selection regression test.
#
# Same staging trick as check-nav.sh: Quickshell will not import a module from
# outside its config folder, so the test is copied in beside components/ and
# services/. HOME is redirected because Settings writes a real dotfile.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"

command -v qs >/dev/null 2>&1 || {
    echo "notif: quickshell (qs) not found, skipping notification test" >&2
    exit 0
}

box="$(mktemp -d)"
trap 'rm -rf "$box"' EXIT
mkdir -p "$box/cfg" "$box/home"
cp -a "$root/components" "$root/services" "$box/cfg/"
cp "$root/tests/notifhistory.qml" "$box/cfg/shell.qml"

# Qt.quit() does not stop Quickshell, so the run is bounded and judged by what
# it printed rather than by its exit status.
out="$(HOME="$box/home" XDG_CONFIG_HOME="$box/home/.config" \
    XDG_CACHE_HOME="$box/home/.cache" XDG_STATE_HOME="$box/home/.state" \
    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    timeout 90 qs -p "$box/cfg" 2>&1 | sed 's/\x1b\[[0-9;]*m//g')"  || true

if grep -q "RESULT: all notification-history tests passed" <<<"$out"; then
    echo "notif: $(grep -c '  ok:' <<<"$out") notification-history assertions passed"
    exit 0
fi

echo "notif: notification-history test FAILED" >&2
grep -E "FAIL|RESULT|ERROR|caused by" <<<"$out" >&2 || echo "$out" >&2
exit 1
