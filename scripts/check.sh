#!/usr/bin/env bash
# Best-effort QML check for paw2-shell — run before committing QML changes.
#
# ponytail: this is qmllint, not a runtime test. It catches syntax errors,
# bad imports, and broken bindings — not behavioral breakage (the shell still
# has to be smoke-tested live with `qs -c paw2-shell`). The `unqualified` check
# is disabled because this shell deliberately uses outer ids (root.bar.x); that
# category is pure noise here, the rest still flags real problems.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"

# Keep secrets, local assets, and obvious machine/private data out of commits.
tracked_local="$(git -C "$root" ls-files -- weather.conf wallpapers)"
if [[ -n "$tracked_local" ]]; then
    echo "privacy: runtime/local files are tracked:" >&2
    echo "$tracked_local" >&2
    exit 1
fi

weather_location="$(git -C "$root" grep -n -E '"(city|country)"[[:space:]]*:[[:space:]]*"[^"]+"' -- settings.json || true)"
if [[ -n "$weather_location" ]]; then
    echo "privacy: settings.json contains a weather location:" >&2
    echo "$weather_location" >&2
    exit 1
fi

home_pattern='(^|[[:space:]"=])/'"home/"'[A-Za-z0-9._-]+'
private_key='PRIVATE KEY'
key_pattern="BEGIN (RSA |OPENSSH |EC |PGP )?$private_key"
private_matches="$(git -C "$root" grep -n -I -E "$home_pattern|$key_pattern" -- . || true)"
if [[ -n "$private_matches" ]]; then
    echo "privacy: possible private data in tracked files:" >&2
    echo "$private_matches" >&2
    exit 1
fi
echo "privacy: tracked files look clean"

# Fedora ships it as qmllint-qt6 under lib64; Arch as qmllint under lib.
qmllint=""
for c in qmllint qmllint-qt6 /usr/lib64/qt6/bin/qmllint /usr/lib/qt6/bin/qmllint; do
    p="$(command -v "$c" 2>/dev/null || true)"
    [[ -n "$p" && -x "$p" ]] && { qmllint="$p"; break; }
done
[[ -n "$qmllint" ]] || { echo "qmllint not found (install qt6-declarative / qt6-qtdeclarative-devel)" >&2; exit 2; }

# Where Quickshell's own QML modules live, so `import Quickshell` resolves.
qmldir="$(qmake6 -query QT_INSTALL_QML 2>/dev/null || true)"
[[ -d "${qmldir:-}" ]] || for d in /usr/lib64/qt6/qml /usr/lib/qt6/qml; do
    [[ -d "$d" ]] && { qmldir="$d"; break; }
done

fail=0
while IFS= read -r f; do
    if ! "$qmllint" -I "$qmldir" --unqualified disable "$f"; then
        fail=$((fail + 1))
    fi
done < <(find "$root" -name '*.qml' | sort)

if [[ $fail -gt 0 ]]; then
    echo "check: $fail file(s) with qmllint findings" >&2
    exit 1
fi
bash "$root/scripts/check-architecture.sh"
bash "$root/scripts/check-nav.sh"
bash "$root/scripts/check-reorder.sh"
bash "$root/scripts/check-notif.sh"
bash "$root/scripts/check-snapshot.sh"
bash "$root/scripts/check-dock.sh"
bash "$root/scripts/check-revert.sh"
bash "$root/scripts/check-wallpaper.sh"

# paw/Makefile's check-shell only covers paw/scripts/, so these had no gate at
# all. bash -n, not sh -n: they use [[ ]].
for s in "$root"/scripts/*.sh; do
    bash -n "$s" || exit 1
done

echo "check: no qmllint errors (warnings above, if any, are advisory)"
