pragma Singleton

import QtQuick
import "." as Services

/// Shared UI tokens so panels and popups match — radii + animation durations.
/// Radii scale from the user's Radius setting (12 → the classic 12/8/4 look).
QtObject {
    readonly property int radiusSm: Math.round(Services.Settings.barRadius / 3)
    readonly property int radiusMd: Math.round(Services.Settings.barRadius * 2 / 3)
    readonly property int radiusLg: Services.Settings.barRadius

    readonly property int durOpen: 180
    readonly property int durClose: 120

    /// The OverlayPanel currently holding exclusive keyboard focus, or null.
    /// Kept here rather than in the panels because it is one seat-wide fact:
    /// the compositor gives keys to a single layer surface, so a second panel
    /// opening has to take the first one down. OverlayPanel maintains it.
    property var exclusivePanel: null

    /// Clamp arrow-key navigation over a grid of `count` items in `cols`
    /// columns. dir: "left"|"right"|"up"|"down". Clamps rather than wraps, so
    /// holding a direction settles at an edge instead of jumping across.
    /// Shared by the launcher's three grids and the welcome screen.
    function gridStep(idx, count, cols, dir) {
        if (dir === "left")  return Math.max(0, idx - 1);
        if (dir === "right") return Math.min(count - 1, idx + 1);
        if (dir === "up")    return idx >= cols ? idx - cols : idx;
        return idx + cols < count ? idx + cols : idx;   // "down"
    }
}
