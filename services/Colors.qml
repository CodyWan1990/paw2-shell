pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io
import "." as Services

/// Color provider — reads themes/<slug>.json and adapts its 14-key Panda palette into 16 indexed colors; recolors reactively on theme/slug change.
QtObject {
    readonly property color background: internal.palette.background
    readonly property color foreground: internal.palette.foreground
    readonly property color cursor: internal.palette.cursor

    readonly property color color0: internal.palette.color0
    readonly property color color1: internal.palette.color1
    readonly property color color2: internal.palette.color2
    readonly property color color3: internal.palette.color3
    readonly property color color4: internal.palette.color4
    readonly property color color5: internal.palette.color5
    readonly property color color6: internal.palette.color6
    readonly property color color7: internal.palette.color7
    readonly property color color8: internal.palette.color8
    readonly property color color9: internal.palette.color9
    readonly property color color10: internal.palette.color10
    readonly property color color11: internal.palette.color11
    readonly property color color12: internal.palette.color12
    readonly property color color13: internal.palette.color13
    readonly property color color14: internal.palette.color14
    readonly property color color15: internal.palette.color15

    readonly property color accent: color4

    // Semantic aliases (matching waybar style.css mapping)
    readonly property color workspaceActive: color1
    readonly property color clockUnderline: color2
    readonly property color updatesUnderline: color3
    readonly property color weatherUnderline: color4
    readonly property color mprisUnderline: color5
    readonly property color cpuUnderline: color6
    readonly property color memoryUnderline: color9
    readonly property color pulseaudioUnderline: color10
    readonly property color notifUnderline: color11
    readonly property color trayUnderline: color12
    readonly property color powermenuUnderline: color13
    readonly property color diskUnderline: color14
    readonly property color networkUnderline: color3
    readonly property color uptimeUnderline: color7
    readonly property color brightnessUnderline: color15
    readonly property color menuUnderline: color8
    readonly property color batteryUnderline: color2

    // Status colors — mapped to the theme palette so they recolor with the theme
    // (color2/color3/color1 = the palette's green/yellow/red).
    readonly property color green: color2
    readonly property color yellow: color3
    readonly property color orange: Qt.rgba(
        (color1.r + color3.r) / 2, (color1.g + color3.g) / 2, (color1.b + color3.b) / 2, 1)
    readonly property color red: color1

    // Resolve a palette key to a color; "auto"/unknown returns fallback (widget's semantic default).
    function byKey(key, fallback) {
        switch (key) {
        case "background": return background;
        case "foreground": return foreground;
        case "cursor": return cursor;
        case "color0": return color0;   case "color1": return color1;
        case "color2": return color2;   case "color3": return color3;
        case "color4": return color4;   case "color5": return color5;
        case "color6": return color6;   case "color7": return color7;
        case "color8": return color8;   case "color9": return color9;
        case "color10": return color10; case "color11": return color11;
        case "color12": return color12; case "color13": return color13;
        case "color14": return color14; case "color15": return color15;
        default: return fallback;
        }
    }

    function withAlpha(c, a) { return Qt.rgba(c.r, c.g, c.b, a); }
    function accentAlpha(a) { return Qt.rgba(accent.r, accent.g, accent.b, a); }

    /// Status color for a value at ascending thresholds:
    /// value < t1 → green, < t2 → yellow, < t3 → orange, else red.
    function threshold(value, t1, t2, t3) {
        if (value < t1) return green;
        if (value < t2) return yellow;
        if (value < t3) return orange;
        return red;
    }

    readonly property var paletteKeys: [
        "auto", "foreground", "background",
        "color1", "color2", "color3", "color4", "color5", "color6", "color7",
        "color8", "color9", "color10", "color11", "color12", "color13", "color14", "color15"
    ]

    readonly property QtObject internal: QtObject {
        readonly property string themesDir: Services.Paths.themesDir

        // Single source of truth for the active slug is Theme.currentSlug
        // (which watches the state file); fall back to Dracula pre-load.
        readonly property string slug: Services.Theme.currentSlug || "dracula"

        // Fallback = Dracula, so the shell is usable before any theme is applied.
        property var palette: ({
            background: "#282a36", foreground: "#f8f8f2", cursor: "#f8f8f2",
            color0: "#282a36", color1: "#ff5555", color2: "#50fa7b", color3: "#f1fa8c",
            color4: "#bd93f9", color5: "#bd93f9", color6: "#8be9fd", color7: "#f8f8f2",
            color8: "#6272a4", color9: "#ff5555", color10: "#50fa7b", color11: "#f1fa8c",
            color12: "#bd93f9", color13: "#bd93f9", color14: "#8be9fd", color15: "#f8f8f2"
        })

        function hex(v, fallback) {
            if (!v || typeof v !== "string") return fallback;
            return v.startsWith("#") ? v : "#" + v;
        }

        // Panda's 14-key palette → 16 indexed colors (terminal-style mapping).
        function adapt(p) {
            let bg = hex(p.bg, "#282a36");
            let fg = hex(p.fg, "#f8f8f2");
            return {
                background: bg, foreground: fg, cursor: fg,
                color0: bg,
                color1: hex(p.red, fg),
                color2: hex(p.green, fg),
                color3: hex(p.yellow, fg),
                color4: hex(p.blue, fg),
                color5: hex(p.mauve, fg),
                color6: hex(p.cyan, fg),
                color7: fg,
                color8: hex(p.comment, fg),
                color9: hex(p.red, fg),
                color10: hex(p.green, fg),
                color11: hex(p.yellow, fg),
                color12: hex(p.blue, fg),
                color13: hex(p.mauve, fg),
                color14: hex(p.cyan, fg),
                color15: fg
            };
        }

        property var raw: ({})

        // Give paw2 the one colour it draws with, so the focus ring matches the
        // shell's accent. Done here rather than in Theme.apply() so a
        // hand-edited theme file recolors the compositor too, not just the
        // shell.
        //
        // paw v1 took the whole 13-colour palette over IPC because it also drew
        // the root background and a lock backdrop. paw2 draws neither: the
        // wallpaper is a layer-shell client and there is no lock screen, so the
        // focus ring is the entire compositor-side surface of a theme.
        function pushToCompositor() {
            const accent = raw.blue ?? raw.mauve ?? raw.fg;
            if (typeof accent !== "string" || accent.length === 0)
                return;
            Services.PawConfig.set("appearance.focus_ring_color",
                                   accent.startsWith("#") ? accent : "#" + accent);
        }

        function parseTheme() {
            try {
                let text = themeView.text();
                if (!text || text.length === 0) return;
                let json = JSON.parse(text);
                if (json.palette) {
                    raw = json.palette;
                    palette = adapt(json.palette);
                    pushToCompositor();
                }
            } catch (e) {
                console.warn("Colors: failed to parse theme JSON:", e);
            }
        }

        // Watches the active theme JSON itself (live edits recolor instantly).
        property FileView themeView: FileView {
            path: Qt.resolvedUrl("file://" + internal.themesDir + "/" + internal.slug + ".json")
            watchChanges: true
            preload: true
            onFileChanged: this.reload()
            onTextChanged: internal.parseTheme()
            Component.onCompleted: internal.parseTheme()
        }
    }
}
