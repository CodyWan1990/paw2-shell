#!/usr/bin/env bash
# Run tests/wallpaperrestore.qml — the login wallpaper-restore regression test.
#
# Same staging trick as check-snapshot.sh, plus a throwaway HOME carrying the
# state the restore reads: a saved theme slug and the theme it names.
#
# That theme's wallpaper deliberately does not exist. apply-wallpaper.sh checks
# the file before it runs `pkill swaybg`, so a test run cannot take down the
# real session's wallpaper.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"

command -v qs >/dev/null 2>&1 || {
    echo "wallpaper: quickshell (qs) not found, skipping wallpaper-restore test" >&2
    exit 0
}

box="$(mktemp -d)"
trap 'rm -rf "$box"' EXIT
mkdir -p "$box/cfg" "$box/home"

# Paths.themesDir and statePath are both derived from HOME, so the shell under
# test reads these rather than anything of the user's.
shelldir="$box/home/.config/quickshell/paw2-shell"
mkdir -p "$shelldir/themes" "$shelldir/scripts" "$box/home/.local/state/paw2-shell"
cp "$root/scripts/apply-wallpaper.sh" "$shelldir/scripts/"
printf 'testtheme' > "$box/home/.local/state/paw2-shell/current"
cat > "$shelldir/themes/testtheme.json" <<'JSON'
{
  "meta": { "name": "Test Theme", "slug": "testtheme", "dark": true },
  "wallpaper": { "path": "/nonexistent/paw2-test-wallpaper.png", "mode": "fill" },
  "palette": { "bg": "1f2430", "fg": "cbccc6", "blue": "73d0ff" }
}
JSON

cp -a "$root/components" "$root/services" "$box/cfg/"
cp "$root/tests/wallpaperrestore.qml" "$box/cfg/shell.qml"

# Qt.quit() does not stop Quickshell, so the run is bounded and judged by what
# it printed rather than by its exit status.
out="$(env -u PAW2_SOCKET HOME="$box/home" XDG_CONFIG_HOME="$box/home/.config" \
    XDG_CACHE_HOME="$box/home/.cache" XDG_STATE_HOME="$box/home/.state" \
    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    timeout 90 qs -p "$box/cfg" 2>&1 | sed 's/\x1b\[[0-9;]*m//g')" || true

if grep -q "RESULT: all wallpaper-restore tests passed" <<<"$out"; then
    echo "wallpaper: $(grep -c '  ok:' <<<"$out") wallpaper-restore assertions passed"
    exit 0
fi

echo "wallpaper: wallpaper-restore test FAILED" >&2
grep -E "FAIL|RESULT|ERROR|caused by" <<<"$out" >&2 || echo "$out" >&2
exit 1
