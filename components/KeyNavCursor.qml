import QtQuick

/// A keyboard row cursor over an arbitrary subtree of settings controls.
///
/// The stops are collected rather than declared: point `scope` at the item
/// holding the rows and this walks it, so adding a setting stays one line of
/// QML with no navigation bookkeeping. Two shapes are recognised, and both
/// already existed before this drove them:
///
///   leaf       `cursor` (bool) + keyActivate() + optional keyAdjust(delta)
///   composite  `selectableCount` + cursorIndex + activateAt(i)
///              + optional adjustAt(i, delta)   — see MonitorSetup
///
/// A subtree marked `keyNavSkip` is left alone entirely, which is how BarTab
/// keeps its drag-and-drop widget list out of the cursor's way.
///
/// Used by SettingsPage (the body of five settings tabs) and directly by
/// BarTab, whose scrolling column mixes navigable rows with that widget list.
QtObject {
    id: nav

    /// Subtree to walk. Everything below it is fair game unless marked.
    property Item scope: null
    /// Scrolled so the cursor stays visible, when the scope lives in one.
    property Flickable flick: null

    /// Which stop holds the cursor; -1 for none (the mouse-only default).
    property int cursorIndex: -1
    /// Flat stop list: { item, sub } — `sub` indexes within a composite.
    property var stops: []
    readonly property int stopCount: stops.length

    /// Recompute the stop list. Rows appear and vanish as values change — a
    /// `preset` row reveals a custom field, the workspace list fans out per
    /// workspace, Compositor rows stay hidden until paw answers — so this runs
    /// whenever the scope's shape changes, not just once.
    function refresh() {
        if (!scope)
            return;
        let found = [];
        collect(scope, found);
        stops = found;
        if (cursorIndex >= found.length)
            setCursor(found.length - 1);
        else
            paint();
    }

    function collect(node, out) {
        const kids = node.children ?? [];
        for (let i = 0; i < kids.length; i++) {
            const kid = kids[i];
            if (!kid || kid.visible === false || kid.keyNavSkip === true)
                continue;
            if (kid.selectableCount !== undefined) {
                for (let s = 0; s < kid.selectableCount; s++)
                    out.push({ item: kid, sub: s });
                continue;   // composites route their own stops
            }
            if (kid.keyActivate !== undefined) {
                out.push({ item: kid, sub: 0 });
                continue;
            }
            // A wrapper: LabeledSettingRow, Card, a bare Item holding a
            // button, a Loader. Descend, so call sites need no markup.
            if (kid.item !== undefined && kid.item !== null)
                collect(kid.item, out);
            collect(kid, out);
        }
    }

    /// Push the cursor flag down. Composites take an index, leaves a bool, and
    /// everything else is cleared — so no stale highlight survives a rebuild.
    function paint() {
        for (let i = 0; i < stops.length; i++) {
            const stop = stops[i];
            const on = i === cursorIndex;
            if (stop.item.selectableCount !== undefined) {
                if (on)
                    stop.item.cursorIndex = stop.sub;
                else if (stop.item.cursorIndex === stop.sub)
                    stop.item.cursorIndex = -1;
            } else {
                stop.item.cursor = on;
            }
        }
    }

    function setCursor(index) {
        cursorIndex = Math.max(-1, Math.min(stops.length - 1, index));
        paint();
        ensureVisible();
    }

    /// Move by `delta`, clamping. Returns false when it would run off the top,
    /// which is how a host knows to park on its tab row instead.
    function stepCursor(delta) {
        if (stops.length === 0)
            return false;
        if (cursorIndex < 0) {
            setCursor(delta > 0 ? 0 : stops.length - 1);
            return true;
        }
        if (cursorIndex + delta < 0)
            return false;
        setCursor(cursorIndex + delta);
        return true;
    }

    function activate() {
        const stop = stops[cursorIndex];
        if (!stop)
            return;
        if (stop.item.activateAt !== undefined)
            stop.item.activateAt(stop.sub);
        else if (stop.item.keyActivate !== undefined)
            stop.item.keyActivate();
    }

    function adjust(delta) {
        const stop = stops[cursorIndex];
        if (!stop)
            return;
        if (stop.item.adjustAt !== undefined)
            stop.item.adjustAt(stop.sub, delta);
        else if (stop.item.keyAdjust !== undefined)
            stop.item.keyAdjust(delta);
    }

    function clearCursor() {
        cursorIndex = -1;
        paint();
    }

    /// Scroll the cursor's row into view, with a row of margin so it never
    /// sits flush against an edge.
    function ensureVisible() {
        const stop = stops[cursorIndex];
        if (!flick || !stop || flick.height <= 0)
            return;
        const top = stop.item.mapToItem(flick.contentItem, 0, 0).y;
        const bottom = top + (stop.item.height || 40);
        const pad = 24;
        if (top - pad < flick.contentY)
            flick.contentY = Math.max(0, top - pad);
        else if (bottom + pad > flick.contentY + flick.height)
            flick.contentY = Math.min(Math.max(0, flick.contentHeight - flick.height),
                bottom + pad - flick.height);
    }
}
