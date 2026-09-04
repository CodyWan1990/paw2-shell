pragma Singleton

import QtQuick
import Quickshell

/// Installed applications shared by the launcher and dock.
QtObject {
    id: root

    readonly property var entries: DesktopEntries.applications.values
        .filter(entry => !entry.noDisplay && entry.command && entry.command.length > 0)
        .map(entry => ({
            id: entry.id,
            name: entry.name,
            comment: entry.comment,
            keywords: (entry.keywords || []).join(" "),
            icon: entry.icon,
            startupClass: entry.startupClass || "",
            desktop: entry
        }))
        .sort((a, b) => a.name.toLowerCase().localeCompare(b.name.toLowerCase()))

    function normalizedId(value) {
        return String(value || "").toLowerCase().replace(/\.desktop$/, "");
    }

    function entryForAppId(catalog, appId) {
        const wanted = normalizedId(appId);
        if (wanted === "")
            return null;
        for (const entry of catalog)
            if (normalizedId(entry.startupClass) === wanted)
                return entry;
        for (const entry of catalog)
            if (normalizedId(entry.id) === wanted)
                return entry;
        return null;
    }

    function entryForId(catalog, id) {
        const wanted = normalizedId(id);
        for (const entry of catalog)
            if (normalizedId(entry.id) === wanted)
                return entry;
        return null;
    }

    /// Pinned entries first, then one item per unpinned app running on output.
    function buildDockItems(catalog, windows, pinnedIds, output, includeUnpinned = true) {
        const items = [];
        const byKey = ({});

        for (const pinnedId of (pinnedIds || [])) {
            const entry = entryForId(catalog, pinnedId);
            const key = "entry:" + normalizedId(pinnedId);
            if (byKey[key])
                continue;
            const item = {
                key: key,
                entryId: entry ? entry.id : String(pinnedId),
                name: entry ? entry.name : String(pinnedId).replace(/\.desktop$/, ""),
                icon: entry ? entry.icon : "",
                entry: entry,
                windows: [],
                pinned: true
            };
            byKey[key] = item;
            items.push(item);
        }

        for (const window of (windows || [])) {
            if (window.output !== output)
                continue;
            const entry = entryForAppId(catalog, window.appId);
            const key = entry
                ? "entry:" + normalizedId(entry.id)
                : (window.appId ? "app:" + normalizedId(window.appId) : "window:" + window.id);
            let item = byKey[key];
            if (!item) {
                if (!includeUnpinned)
                    continue;
                item = {
                    key: key,
                    entryId: entry ? entry.id : "",
                    name: entry ? entry.name : (window.appId || window.title || "Unknown app"),
                    icon: entry ? entry.icon : "",
                    entry: entry,
                    windows: [],
                    pinned: false
                };
                byKey[key] = item;
                items.push(item);
            }
            item.windows.push(window);
        }
        return items;
    }

    /// Focus the first window, or cycle when this app already owns focus.
    function nextWindow(windows) {
        if (!windows || windows.length === 0)
            return null;
        const focused = windows.findIndex(window => window.focused === true);
        return windows[focused < 0 ? 0 : (focused + 1) % windows.length];
    }
}
