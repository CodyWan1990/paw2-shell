pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io
import "." as Services

/// Settings singleton — persists user preferences to JSON.
/// All properties are writable and trigger a debounced save.
QtObject {
    id: root

    property string barPosition: "top"
    property int barHeight: 35
    property real barOpacity: 1.0
    // Deliberately minimal. The widgets that need configuring or a network
    // before they say anything useful — updates, weather, media, cpu, memory —
    // are off by default; they are one click away in Settings → Bar.
    property var barLeft: ["separator", "menu", "separator", "workspaces", "separator", "layout", "separator", "windowTitle"]
    property var barCenter: ["clock"]
    property var barRight: ["audio", "separator", "themeSwitcher", "separator", "systemTray", "separator", "notifications", "separator", "power"]
    property bool barFloating: false
    // The margin a floating bar keeps from the screen edge. Window gaps are the
    // compositor's own `appearance.gaps`, set on Settings -> Compositor.
    property int barGap: 20
    property int barRadius: 0
    property bool barShadowEnabled: true
    property int barShadowRadius: 12
    property real barShadowOpacity: 0.45
    property string barShadowColor: "#000000"
    property int barShadowOffsetX: 0
    property int barShadowOffsetY: 4
    property bool barHideVacantTags: false

    property bool dockEnabled: true
    property int dockIconSize: 48
    property string dockMode: "reserved"
    property string dockPosition: "bottom"
    property int dockEdgeMargin: 8
    property real dockOpacity: 0.94
    property int dockRadius: 32
    property bool dockBorderEnabled: true
    property bool dockShadowEnabled: true
    property int dockShadowRadius: 12
    property real dockShadowOpacity: 0.45
    property string dockShadowColor: "#000000"
    property int dockShadowOffsetX: 0
    property int dockShadowOffsetY: 4
    property bool dockMagnificationEnabled: true
    property real dockMagnification: 1.2
    property int dockHideDelayMs: 600
    property bool dockShowLauncher: true
    property bool dockShowRunningApps: true
    property bool dockShowIndicators: true
    /// Ordered desktop-entry ids. An empty list bootstraps from running apps.
    property var dockPinnedApps: []

    // Must be a Nerd Font — see fontFamilies below. Cascadia because it is the
    // one Fedora actually packages, so paw.spec can require it.
    property string fontFamily: "Cascadia Code NF"
    property int fontSize: 17

    /// Legacy free-text terminal command. Still read as the fallback when no
    /// desktop entry has been chosen, so an existing settings.json keeps
    /// working; DefaultApps keeps it in step when you pick one.
    property string appTerminal: "kitty"
    /// Desktop-entry ids of the chosen default apps (e.g. "kitty.desktop").
    /// An id rather than a command: it carries the entry's own argv, which is
    /// already parsed and field-code-free.
    property string appTerminalId: ""
    property string appEditorId: ""

    property int maxVisible: 5
    property int autoDismissSeconds: 10
    /// How many notifications the Control Center keeps. They are no longer
    /// closed when their toast times out, so something has to bound the list.
    property int historyLimit: 50

    property string weatherCity: ""
    property string weatherCountry: ""
    property string weatherUnits: "metric"

    /// Where the Wallpapers tab lists images from.
    property string wallpaperDir: Quickshell.env("HOME") + "/Pictures/Wallpapers"
    /// slug -> wallpaper, applied when that theme is activated. An absolute
    /// path is used as-is; a bare filename resolves against wallpaperDir.
    /// Lives here rather than in themes/<slug>.json so a package update cannot
    /// overwrite the user's choice.
    property var themeWallpapers: ({})
    /// Applying a wallpaper also binds it to the active theme.
    property bool bindWallpaperToTheme: true

    /// Whether a theme switch also recolors kitty and GTK apps. Both write
    /// outside paw2-shell's own config — see scripts/apply-theme-externals.sh for
    /// exactly what, and turning one off removes what it wrote.
    property bool themeKitty: true
    property bool themeGtk: true

    /// False until the welcome screen has been dismissed, which is what makes
    /// it show itself on a fresh install: a machine with no settings.json has
    /// never run it. Cleared from the Settings panel to run it again.
    property bool firstRunDone: false

    /// True once settings.json has been read, or ruled absent. Anything that
    /// branches on a stored value must wait for this — before it, every
    /// property above is still the compiled default, so a welcome screen keyed
    /// on firstRunDone would show itself on every start.
    readonly property bool ready: internal.loaded

    // Property name -> dotted path in settings.json. One table drives loading,
    // saving, and the change-handler wiring below, so a new setting is a
    // declaration above plus one row here — not four edits in four places.
    readonly property var schema: ({
        barPosition: "bar.position",
        barHeight: "bar.height",
        barOpacity: "bar.opacity",
        barLeft: "bar.left",
        barCenter: "bar.center",
        barRight: "bar.right",
        barFloating: "bar.floating",
        barGap: "bar.gap",
        barRadius: "bar.radius",
        barShadowEnabled: "bar.shadowEnabled",
        barShadowRadius: "bar.shadowRadius",
        barShadowOpacity: "bar.shadowOpacity",
        barShadowColor: "bar.shadowColor",
        barShadowOffsetX: "bar.shadowOffsetX",
        barShadowOffsetY: "bar.shadowOffsetY",
        barHideVacantTags: "bar.hideVacantTags",
        dockEnabled: "dock.enabled",
        dockIconSize: "dock.iconSize",
        dockMode: "dock.mode",
        dockPosition: "dock.position",
        dockEdgeMargin: "dock.edgeMargin",
        dockOpacity: "dock.opacity",
        dockRadius: "dock.radius",
        dockBorderEnabled: "dock.borderEnabled",
        dockShadowEnabled: "dock.shadowEnabled",
        dockShadowRadius: "dock.shadowRadius",
        dockShadowOpacity: "dock.shadowOpacity",
        dockShadowColor: "dock.shadowColor",
        dockShadowOffsetX: "dock.shadowOffsetX",
        dockShadowOffsetY: "dock.shadowOffsetY",
        dockMagnificationEnabled: "dock.magnificationEnabled",
        dockMagnification: "dock.magnification",
        dockHideDelayMs: "dock.hideDelayMs",
        dockShowLauncher: "dock.showLauncher",
        dockShowRunningApps: "dock.showRunningApps",
        dockShowIndicators: "dock.showIndicators",
        dockPinnedApps: "dock.pinnedApps",
        fontFamily: "general.fontFamily",
        fontSize: "general.fontSize",
        appTerminal: "defaultApps.terminal",
        appTerminalId: "defaultApps.terminalId",
        appEditorId: "defaultApps.editorId",
        maxVisible: "notifications.maxVisible",
        autoDismissSeconds: "notifications.autoDismissSeconds",
        historyLimit: "notifications.historyLimit",
        weatherCity: "weather.city",
        weatherCountry: "weather.country",
        weatherUnits: "weather.units",
        wallpaperDir: "general.wallpaperDir",
        themeWallpapers: "theme.wallpapers",
        bindWallpaperToTheme: "theme.bindOnApply",
        themeKitty: "theme.kitty",
        themeGtk: "theme.gtk",
        firstRunDone: "general.firstRunDone"
    })

    // These three hold widget-instance arrays, not scalars, so they go through
    // normalizeSection() on the way in.
    readonly property var sectionProps: ["barLeft", "barCenter", "barRight"]

    // Structured values. Like sectionProps they are `var`, so `!==` compares
    // by reference and would reassign on every load; they need a value compare.
    readonly property var objectProps: ["dockPinnedApps", "themeWallpapers"]

    function isDockAppPinned(id) {
        if (!id)
            return false;
        const wanted = String(id).toLowerCase();
        return dockPinnedApps.some(pinned => String(pinned).toLowerCase() === wanted);
    }

    function pinDockApp(id) {
        if (!id || isDockAppPinned(id))
            return;
        dockPinnedApps = dockPinnedApps.concat([id]);
    }

    function unpinDockApp(id) {
        const wanted = String(id).toLowerCase();
        dockPinnedApps = dockPinnedApps.filter(pinned => String(pinned).toLowerCase() !== wanted);
    }

    function toggleDockApp(id) {
        if (isDockAppPinned(id))
            unpinDockApp(id);
        else
            pinDockApp(id);
    }

    /// Move a pin to an insertion point in the ordered pinned-app list.
    function moveDockApp(id, toIndex) {
        const wanted = String(id).toLowerCase();
        const fromIndex = dockPinnedApps.findIndex(
            pinned => String(pinned).toLowerCase() === wanted);
        if (fromIndex < 0)
            return;
        const bounded = Math.max(0, Math.min(toIndex, dockPinnedApps.length));
        if (bounded === fromIndex || bounded === fromIndex + 1)
            return;

        let next = dockPinnedApps.slice();
        const app = next.splice(fromIndex, 1)[0];
        const adjusted = bounded > fromIndex ? bounded - 1 : bounded;
        next.splice(adjusted, 0, app);
        dockPinnedApps = next;
    }

    /// Bind (or with an empty path, unbind) a wallpaper for a theme slug.
    /// Clones the map: `var` properties compare by reference, so mutating in
    /// place emits no change signal and never reaches scheduleSave().
    function setThemeWallpaper(slug, path) {
        if (!slug) return;
        let next = Object.assign({}, themeWallpapers);
        if (path) next[slug] = path; else delete next[slug];
        themeWallpapers = next;
    }

    // Bar sections are arrays of widget instance objects: { id, type, style, color, ... };
    // normalizeSection() upgrades legacy string entries transparently on load.
    function genWidgetId() { return "w" + Math.random().toString(36).slice(2, 8); }

    function defaultStyleFor(type) {
        return (type === "separator" || type === "spacer"
                || type === "windowTitle" || type === "workspaces") ? "none" : "underline";
    }

    // Per-widget-type options, beyond the shape/color/icon-set every widget has.
    // One table drives three things: the rows BarTab's editor renders, the keys
    // normalizeWidget() lets through a save/load round-trip, and the fallback a
    // widget uses when its key is unset — so a new option is one row here plus
    // one `instance.key ?? fallback` read in the widget.
    //
    // Row types: bool | int | float | preset | palette | text | note.
    //   int/float  from, to, step, decimals, suffix; `scale` stores value*scale
    //   preset     choices [{ value, label }]; allowCustom (default true) adds a
    //              "Custom…" entry revealing a raw field; `hint` documents the
    //              placeholders a template row accepts
    //   palette    a Colors.paletteKeys dropdown; "auto" uses the widget fallback
    //   note       static explanatory text, no key
    readonly property var widgetSpecs: ({
        workspaces: [
            { key: "format", label: "Label", type: "preset", fallback: "{indicator} {name}",
              hint: "{index} {name} {indicator}",
              choices: [{ value: "{indicator} {name}", label: "Indicator and name" },
                        { value: "{name}", label: "Name only" },
                        { value: "{index}", label: "Number only" },
                        { value: "{indicator}", label: "Indicator only" }] },
            { key: "indicator", label: "Indicator", type: "text", fallback: "" },
            { key: "tagScale", label: "Tag size", type: "float",
              from: 0.8, to: 2.5, step: 0.1, decimals: 1, suffix: "×", fallback: 1.3 },
            { key: "itemSpacing", label: "Tag spacing", type: "int",
              from: 0, to: 20, suffix: "px", fallback: 4 },
            { key: "tagPadding", label: "Tag padding", type: "int",
              from: 0, to: 24, suffix: "px", fallback: 4 },
            { key: "bold", label: "Bold labels", type: "bool", fallback: true },
            { key: "underlineHeight", label: "Focus underline", type: "int",
              from: 0, to: 8, suffix: "px", fallback: 2 },
            { key: "activeColor", label: "Active color", type: "palette", fallback: "auto" },
            { key: "urgentColor", label: "Urgent color", type: "palette", fallback: "auto" },
            { key: "hoverColor", label: "Hover color", type: "palette", fallback: "auto" },
            { key: "occupiedColor", label: "Occupied color", type: "palette", fallback: "auto" },
            { key: "vacantColor", label: "Vacant color", type: "palette", fallback: "auto" },
            { key: "vacantMode", label: "Vacant tags", type: "preset",
              fallback: "inherit", allowCustom: false,
              choices: [{ value: "inherit", label: "Inherit bar default" },
                        { value: "show", label: "Always show" },
                        { value: "hide", label: "Always hide" }] },
            { key: "scrollEnabled", label: "Wheel switching", type: "bool", fallback: false },
            { key: "reverseScroll", label: "Reverse wheel", type: "bool", fallback: false }
        ],

        clock: [
            { key: "weekdayFormat", label: "Weekday", type: "preset", fallback: "dddd",
              hint: "Qt date format tokens",
              choices: [{ value: "dddd", label: "Monday" }, { value: "ddd", label: "Mon" },
                        { value: "", label: "Hidden" }] },
            { key: "dateFormat", label: "Date", type: "preset", fallback: "MMMM d, yyyy",
              hint: "Qt date format tokens",
              choices: [{ value: "MMMM d, yyyy", label: "January 5, 2026" },
                        { value: "MMM d", label: "Jan 5" },
                        { value: "yyyy-MM-dd", label: "2026-01-05" },
                        { value: "d/M/yyyy", label: "5/1/2026" },
                        { value: "", label: "Hidden" }] },
            { key: "timeFormat", label: "Time", type: "preset", fallback: "hh:mm AP",
              hint: "Qt time format tokens; include \"ss\" to tick per second",
              choices: [{ value: "hh:mm AP", label: "03:04 PM" },
                        { value: "h:mm AP", label: "3:04 PM" },
                        { value: "HH:mm", label: "15:04" },
                        { value: "HH:mm:ss", label: "15:04:05" },
                        { value: "", label: "Hidden" }] }
        ],

        cpu: [
            { key: "format", label: "Text", type: "preset", fallback: "{ghz}GHz ({pct}){temp}",
              hint: "{ghz} {pct} {temp} {cores} — {pct} and {temp} carry their own unit",
              choices: [{ value: "{ghz}GHz ({pct}){temp}", label: "2.40GHz (12%) 45°C" },
                        { value: "{pct}{temp}", label: "12% 45°C" },
                        { value: "{pct}", label: "12%" },
                        { value: "{ghz}GHz", label: "2.40GHz" }] },
            { key: "showTemp", label: "Temperature", type: "bool", fallback: true },
            { key: "warnPct", label: "Yellow above", type: "int", from: 0, to: 100, suffix: "%", fallback: 50 },
            { key: "highPct", label: "Orange above", type: "int", from: 0, to: 100, suffix: "%", fallback: 65 },
            { key: "critPct", label: "Red above", type: "int", from: 0, to: 100, suffix: "%", fallback: 80 }
        ],

        memory: [
            { key: "format", label: "Text", type: "preset", fallback: "{used}/{total}GB ({pct})",
              hint: "{used} {total} {free} {pct} — {pct} carries its own % sign",
              choices: [{ value: "{used}/{total}GB ({pct})", label: "12/32GB (38%)" },
                        { value: "{pct}", label: "38%" },
                        { value: "{used}GB", label: "12GB" },
                        { value: "{free}GB free", label: "20GB free" }] },
            { key: "warnPct", label: "Yellow above", type: "int", from: 0, to: 100, suffix: "%", fallback: 50 },
            { key: "highPct", label: "Orange above", type: "int", from: 0, to: 100, suffix: "%", fallback: 65 },
            { key: "critPct", label: "Red above", type: "int", from: 0, to: 100, suffix: "%", fallback: 80 }
        ],

        sysMonitor: [
            { key: "format", label: "CPU text", type: "preset", fallback: "{ghz}GHz ({pct}){temp}",
              hint: "{ghz} {pct} {temp} {cores} — {pct} and {temp} carry their own unit",
              choices: [{ value: "{ghz}GHz ({pct}){temp}", label: "2.40GHz (12%) 45°C" },
                        { value: "{pct}{temp}", label: "12% 45°C" },
                        { value: "{pct}", label: "12%" }] },
            { key: "memFormat", label: "Memory text", type: "preset", fallback: "{used}/{total}GB ({pct})",
              hint: "{used} {total} {free} {pct} — {pct} carries its own % sign",
              choices: [{ value: "{used}/{total}GB ({pct})", label: "12/32GB (38%)" },
                        { value: "{pct}", label: "38%" },
                        { value: "{used}GB", label: "12GB" }] },
            { key: "showTemp", label: "CPU temperature", type: "bool", fallback: true }
        ],

        diskUsage: [
            { key: "path", label: "Mount point", type: "text", fallback: "/" },
            { key: "format", label: "Text", type: "preset", fallback: "{used}/{total}GB ({pct})",
              hint: "{used} {total} {free} {pct} — {pct} carries its own % sign",
              choices: [{ value: "{used}/{total}GB ({pct})", label: "120/500GB (24%)" },
                        { value: "{pct}", label: "24%" },
                        { value: "{free}GB free", label: "380GB free" },
                        { value: "{used}GB", label: "120GB" }] },
            { key: "intervalSec", label: "Refresh", type: "int", from: 5, to: 600, suffix: "s", fallback: 60 },
            { key: "warnPct", label: "Yellow above", type: "int", from: 0, to: 100, suffix: "%", fallback: 60 },
            { key: "highPct", label: "Orange above", type: "int", from: 0, to: 100, suffix: "%", fallback: 80 },
            { key: "critPct", label: "Red above", type: "int", from: 0, to: 100, suffix: "%", fallback: 90 }
        ],

        network: [
            { key: "format", label: "Text", type: "preset", fallback: "↓{rx} ↑{tx}",
              hint: "{rx} {tx}",
              choices: [{ value: "↓{rx} ↑{tx}", label: "↓1.2M ↑300K" },
                        { value: "{rx} / {tx}", label: "1.2M / 300K" },
                        { value: "↓{rx}", label: "Download only" },
                        { value: "↑{tx}", label: "Upload only" }] }
        ],

        uptime: [
            { key: "format", label: "Text", type: "preset", fallback: "auto", allowCustom: false,
              choices: [{ value: "auto", label: "3d 4h 20m (skip zero units)" },
                        { value: "dhm", label: "0d 4h 20m (always)" },
                        { value: "hm", label: "76h 20m" },
                        { value: "d", label: "3d" }] },
            { key: "intervalSec", label: "Refresh", type: "int", from: 10, to: 3600, suffix: "s", fallback: 60 }
        ],

        battery: [
            { key: "showTime", label: "Time remaining", type: "bool", fallback: false },
            { key: "critPct", label: "Red at or below", type: "int", from: 0, to: 100, suffix: "%", fallback: 15 },
            { key: "lowPct", label: "Orange at or below", type: "int", from: 0, to: 100, suffix: "%", fallback: 30 },
            { key: "warnPct", label: "Yellow at or below", type: "int", from: 0, to: 100, suffix: "%", fallback: 50 }
        ],

        brightness: [
            { key: "command", label: "Backend", type: "text", fallback: "brightnessctl" },
            { key: "minPercent", label: "Minimum", type: "int", from: 0, to: 50, suffix: "%", fallback: 1 },
            { key: "intervalSec", label: "Refresh", type: "int", from: 1, to: 120, suffix: "s", fallback: 5 }
        ],

        audio: [
            { key: "showPercent", label: "Show percentage", type: "bool", fallback: true },
            { key: "muteLabel", label: "Muted text", type: "text", fallback: "MUTE" },
            { key: "scrollStep", label: "Scroll step", type: "int", from: 1, to: 25, suffix: "%", fallback: 5 },
            { key: "maxVolume", label: "Max volume", type: "int", from: 100, to: 150, suffix: "%", fallback: 100 }
        ],

        media: [
            { key: "format", label: "Text", type: "preset", fallback: "{artist} - {title}",
              hint: "{artist} {title} {album} {player}",
              choices: [{ value: "{artist} - {title}", label: "Artist - Title" },
                        { value: "{title}", label: "Title only" },
                        { value: "{title} — {artist}", label: "Title — Artist" },
                        { value: "{player}: {title}", label: "Player: Title" }] },
            { key: "maxChars", label: "Max characters", type: "int", from: 8, to: 120, fallback: 35 },
            { key: "maxWidth", label: "Max width", type: "int", from: 80, to: 600, suffix: "px", fallback: 250 },
            { key: "showControls", label: "Transport buttons", type: "bool", fallback: true }
        ],

        systemTray: [
            { key: "iconSize", label: "Icon size", type: "int", from: 10, to: 40, suffix: "px", fallback: 20 },
            { key: "itemSpacing", label: "Spacing", type: "int", from: 0, to: 20, suffix: "px", fallback: 4 },
            { key: "hideWhenEmpty", label: "Hide when empty", type: "bool", fallback: true }
        ],

        updates: [
            { key: "hideWhenNone", label: "Hide when up to date", type: "bool", fallback: false }
        ],

        weather: [
            { key: "showCity", label: "Show city", type: "bool", fallback: false },
            { key: "showUnit", label: "Show unit symbol", type: "bool", fallback: true }
        ],

        themeSwitcher: [
            { key: "maxPopupHeight", label: "Popup height", type: "int",
              from: 120, to: 800, suffix: "px", fallback: 360 }
        ],

        layout: [
            { key: "display", label: "Display", type: "preset", fallback: "symbol", allowCustom: false,
              choices: [{ value: "symbol", label: "Symbol ([]=)" },
                        { value: "name", label: "Name (Master)" }] }
        ],

        windowTitle: [
            { key: "maxWidth", label: "Max width", type: "int", from: 80, to: 900, suffix: "px", fallback: 360 },
            { key: "prefix", label: "Prefix", type: "text", fallback: "| " }
        ],

        notifications: [
            { key: "hideWhenZero", label: "Hide when none", type: "bool", fallback: false }
        ],

        menu: [
            { key: "glyph", label: "Glyph", type: "text", fallback: "☰" }
        ],

        separator: [
            { key: "width", label: "Width", type: "int", from: 2, to: 60, suffix: "px", fallback: 12 },
            { key: "glyph", label: "Glyph", type: "text", fallback: "" }
        ],

        spacer: [
            { key: "width", label: "Width", type: "int", from: 4, to: 400, suffix: "px", fallback: 50 }
        ],

        custom: [
            { type: "note", text: "Runs as your user via sh -c. Output's first line is shown." },
            { key: "label", label: "Label", type: "text", fallback: "" },
            { key: "icon", label: "Icon", type: "text", fallback: "" },
            { key: "command", label: "Command", type: "text", fallback: "" },
            { key: "onClick", label: "On-click", type: "text", fallback: "" },
            { key: "intervalMs", label: "Interval", type: "int",
              from: 1, to: 60, suffix: "s", scale: 1000, fallback: 5000 }
        ]
    })

    // Every option key any widget type declares. normalizeWidget() carries these
    // through a round-trip; anything else in a stored instance is dropped. Derived
    // rather than hand-listed — a hand-listed one silently ate new keys.
    readonly property var widgetKeys: {
        let seen = ({});
        for (const type in widgetSpecs)
            for (const spec of widgetSpecs[type])
                if (spec.key !== undefined) seen[spec.key] = true;
        return Object.keys(seen);
    }

    // Font pickers list Nerd Fonts first: every bar icon is a Nerd Font
    // codepoint, so anything else renders the widgets as tofu.
    readonly property var fontFamilies: {
        const all = Qt.fontFamilies();
        const nerd = /nerd font|\bNF[MP]?\b/i;
        return all.filter(f => nerd.test(f)).concat(all.filter(f => !nerd.test(f)));
    }

    function normalizeWidget(el) {
        if (el && typeof el === "object") {
            let out = {
                id: el.id || genWidgetId(),
                type: el.type,
                style: el.style || defaultStyleFor(el.type),
                color: el.color || "auto",
                iconSet: el.iconSet || "emoji"
            };
            root.widgetKeys.forEach(function (k) {
                if (el[k] !== undefined) out[k] = el[k];
            });
            return out;
        }
        // String form (used by the built-in section defaults above)
        let s = String(el);
        return { id: genWidgetId(), type: s, style: defaultStyleFor(s), color: "auto", iconSet: "emoji" };
    }

    function normalizeSection(arr) {
        if (!Array.isArray(arr)) return [];
        return arr.map(normalizeWidget);
    }

    // Normalize string-form section arrays at startup so the model always sees instance objects.
    Component.onCompleted: {
        for (const p of sectionProps)
            root[p] = normalizeSection(root[p]);
        // Every setting saves the same way, so wire the change signals in a
        // loop rather than writing eighteen identical handlers.
        for (const p in schema)
            root[p + "Changed"].connect(internal.scheduleSave);
    }

    readonly property QtObject internal: QtObject {
        id: _internal
        property string settingsPath: Services.Paths.settingsPath
        property bool loaded: false
        property bool saving: false

        property FileView fileView: FileView {
            path: Qt.resolvedUrl("file://" + internal.settingsPath)
            watchChanges: true
            preload: true
            atomicWrites: true
            onTextChanged: {
                // setText() fires this on our own save. Re-loading it would hand
                // loadFromText a fresh copy of what we just wrote — see the
                // identity note there for why that is not harmless.
                if (internal.saving) {
                    internal.loaded = true;
                    return;
                }
                internal.loadFromText(this.text());
                internal.loaded = true;
            }
            onFileChanged: {
                // Ignore file changes triggered by our own save
                if (internal.saving) return;
                this.reload();
                internal.loadFromText(this.text());
            }
            // Clear the saving guard once our own write lands (was the saveProc callback).
            onSaved: internal.savingClearTimer.start()
            onSaveFailed: function (error) {
                console.warn("Settings: save failed:", error);
                internal.saving = false;
            }
        }

        property Timer saveTimer: Timer {
            interval: 500
            onTriggered: internal.saveToFile()
        }

        function scheduleSave() {
            if (!loaded || loading) return;
            saveTimer.restart();
        }

        property bool loading: false

        // Walk a dotted path into obj; undefined if any segment is missing.
        function dig(obj, path) {
            return path.split(".").reduce((o, k) =>
                (o === undefined || o === null) ? undefined : o[k], obj);
        }

        function loadFromText(raw) {
            if (!raw || raw.length === 0) {
                console.log("Settings: no file found, using defaults");
                return;
            }

            try {
                loading = true;
                let j = JSON.parse(raw);
                for (const prop in root.schema) {
                    let v = dig(j, root.schema[prop]);
                    if (v === undefined) continue;   // absent key keeps the default
                    if (root.sectionProps.indexOf(prop) >= 0) {
                        // normalizeSection mints new objects every call, and `var`
                        // properties compare by reference — so assigning an equal
                        // array still emits barLeftChanged. That resets TopBar's
                        // Repeaters, which destroys and rebuilds every widget on
                        // every monitor. Only assign when the content really differs.
                        let next = normalizeSection(v);
                        if (JSON.stringify(next) !== JSON.stringify(root[prop]))
                            root[prop] = next;
                    } else if (root.objectProps.indexOf(prop) >= 0) {
                        // Same reference-vs-value problem as sectionProps, minus
                        // the normalization.
                        if (JSON.stringify(v) !== JSON.stringify(root[prop]))
                            root[prop] = v;
                    } else if (root[prop] !== v) {
                        root[prop] = v;
                    }
                }
                loading = false;
                console.log("Settings: loaded from", settingsPath);
            } catch (e) {
                loading = false;
                console.warn("Settings: failed to parse JSON:", e);
            }
        }

        function toJson() {
            let out = {};
            for (const prop in root.schema) {
                let parts = root.schema[prop].split(".");
                let leaf = parts.pop();
                let node = parts.reduce((o, k) => (o[k] = o[k] || {}), out);
                node[leaf] = root[prop];
            }
            return JSON.stringify(out, null, 2);
        }

        property Timer savingClearTimer: Timer {
            // Delay clearing the saving flag so FileView's onFileChanged can check it first.
            interval: 200
            onTriggered: internal.saving = false
        }

        function saveToFile() {
            saving = true;
            fileView.setText(toJson());  // native atomic write; onSaved clears `saving`
        }
    }

    // Bootstrap timer — only saves defaults if file didn't load after 1 second
    property Timer bootstrapTimer: Timer {
        interval: 1000
        running: true
        onTriggered: {
            if (!internal.loaded) {
                internal.loaded = true;
                internal.scheduleSave();
                console.log("Settings: bootstrapping defaults (no file found)");
            }
        }
    }
}
