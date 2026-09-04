#!/usr/bin/env bash
# Stable ownership checks for QML patterns that have intentionally been shared.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
fail=0

problem() {
    echo "architecture: $*" >&2
    fail=$((fail + 1))
}

mapfile -t mpris_owners < <(rg -l '^import Quickshell\.Services\.Mpris' "$root" --glob '*.qml' | sort)
[[ ${#mpris_owners[@]} -eq 1 && ${mpris_owners[0]} == "$root/services/Media.qml" ]] \
    || problem "services/Media.qml must be the only direct MPRIS importer"

mapfile -t safe_markup_owners < <(rg -l 'Fmt\.(safeMarkup|openableUrl)' "$root" --glob '*.qml' | sort)
[[ ${#safe_markup_owners[@]} -eq 1 && ${safe_markup_owners[0]} == "$root/components/NotificationContent.qml" ]] \
    || problem "components/NotificationContent.qml must own notification markup and URL handling"

[[ $(rg -c 'Components\.BarSection[[:space:]]*\{' "$root/panels/TopBar.qml") -eq 3 ]] \
    || problem "TopBar must contain exactly three shared BarSection instances"
rg -q 'delegate:[[:space:]]*Loader' "$root/panels/TopBar.qml" \
    && problem "TopBar contains an inline widget Loader instead of BarSection"

for tab in AppsTab DockTab GeneralTab WeatherTab CompositorTab MonitorsTab; do
    rg -q '^Components\.SettingsPage[[:space:]]*\{' "$root/panels/settings/$tab.qml" \
        || problem "settings/$tab.qml must use SettingsPage"
done


[[ $(rg -l '(Components\.)?MonitorSetup[[:space:]]*\{' "$root" --glob '*.qml' | wc -l) -ge 2 ]] \
    || problem "monitor setup must remain shared by onboarding and settings"

rg -q '^[[:space:]]*component[[:space:]]+[A-Za-z]' "$root/panels" "$root/widgets" --glob '*.qml' \
    && problem "panels/widgets contain a named inline component that belongs in components/"

rg -q 'id:[[:space:]]*(boolComp|sliderComp|paletteComp|presetComp|textComp|noteComp)' \
    "$root/panels/settings/BarTab.qml" "$root/panels/settings/CompositorTab.qml" \
    && problem "settings tabs contain obsolete inline option editors"

rg -q 'launcher\.gridStep|function[[:space:]]+gridStep' "$root/panels/AppLauncher.qml" "$root/panels/launcher" --glob '*.qml' \
    && problem "launcher views must consume Ui.gridStep directly"

# Quickshell 0.3 segfaults in PopupAnchor::onItemWindowChanged when a popup is
# bound to a reactive delegate that gets destroyed. Dock and launcher popups
# copy item coordinates but keep their anchors on stable panel windows.
rg -q 'anchor\.item' "$root/panels/Dock.qml" "$root/panels/launcher/AppsView.qml" \
    && problem "reactive panel delegates must not own popup anchors"

while IFS= read -r component; do
    name="$(basename "$component" .qml)"
    if ! rg -q "(Components\.)?${name}[[:space:]]*\\{" "$root" --glob '*.qml'; then
        problem "components/$name.qml has no QML consumer"
    fi
done < <(find "$root/components" -maxdepth 1 -name '*.qml' | sort)

if [[ $fail -gt 0 ]]; then
    echo "architecture: $fail ownership check(s) failed" >&2
    exit 1
fi
echo "architecture: shared-component ownership checks passed"
