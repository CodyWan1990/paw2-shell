pragma Singleton

import QtQuick
import Quickshell.Io
import "." as Services

/// Theme registry + switcher — drop a JSON file in themes/ and it appears here; active slug persists to ~/.local/state/paw2-shell/current (watched by Colors.qml).
QtObject {
    id: root

    readonly property string themesDir: Services.Paths.themesDir
    readonly property string statePath: Services.Paths.statePath

    /// [{ name, slug, swatches, wallpaper }] discovered from the themes folder, sorted by name.
    property var themes: []
    /// Currently applied theme slug.
    property string currentSlug: ""

    /// Re-scan the themes folder.
    function refresh() { internal.listProc.running = true; }

    /// Save a new theme (name + palette) to themes/<slug>.json, then apply it.
    /// Pure-JS port of the old python saveScript (slug + palette normalization + luminance).
    function saveTheme(name, palette) {
        name = (name || "").trim();
        let slug = name.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "");
        if (!slug) slug = "custom";

        let p = {};
        for (let k in palette) p[k] = String(palette[k]).replace(/^#/, "");
        if (p.lavender === undefined)  p.lavender = p.mauve || "";
        if (p.maroon === undefined)    p.maroon = p.red || "";
        if (p.orange === undefined)    p.orange = p.yellow || "";
        if (p.teal === undefined)      p.teal = p.cyan || "";
        if (p.selection === undefined) p.selection = p.comment || "";

        let bg = p.bg || "000000";
        let lum = 0;
        try {
            lum = (parseInt(bg.slice(0, 2), 16) * 0.299
                 + parseInt(bg.slice(2, 4), 16) * 0.587
                 + parseInt(bg.slice(4, 6), 16) * 0.114) / 255;
        } catch (e) { lum = 0; }

        let obj = { meta: { name: name || slug, slug: slug, dark: lum < 0.5 }, palette: p };
        internal.pendingApplySlug = slug;
        internal.saveView.path = Qt.resolvedUrl("file://" + root.themesDir + "/" + slug + ".json");
        internal.saveView.setText(JSON.stringify(obj, null, 2));
    }

    /// Apply a theme by slug: persist it, set the wallpaper, re-theme external apps (kitty + GTK).
    function apply(slug) {
        root.currentSlug = slug;
        // This *is* the wallpaper being set, so the startup restore below has
        // nothing left to do.
        internal.wallpaperRestored = true;
        // Deliberately a shell call, not currentView.setText(): Quickshell's
        // FileView cannot create missing parent directories, and on a fresh
        // machine ~/.local/state/paw2-shell/ does not exist yet.
        internal.writeProc.command = ["sh", "-c",
            "mkdir -p \"$(dirname \"$1\")\" && printf '%s' \"$2\" > \"$1\"",
            "sh", root.statePath, slug];
        internal.writeProc.running = true;
        internal.applyWallpaper(slug);
        internal.applyExternals(slug);
    }

    /// Re-run the externals script for the active theme, without touching the
    /// wallpaper. What the kitty and GTK toggles call, so turning one off
    /// actually removes what it wrote instead of waiting for the next switch.
    function reapplyExternals() {
        if (root.currentSlug)
            internal.applyExternals(root.currentSlug);
    }

    readonly property QtObject internal: QtObject {
        id: priv

        // slug -> { slug, name, swatches, wallpaper }; populated by listProc below.
        property var themeMap: ({})
        property string pendingApplySlug: ""

        function rebuild() {
            let arr = Object.keys(themeMap).map(k => themeMap[k]);
            arr.sort((a, b) => a.name.toLowerCase().localeCompare(b.name.toLowerCase()));
            root.themes = arr;
            restoreWallpaper();
        }

        /// Whether the wallpaper has been put back this session.
        property bool wallpaperRestored: false

        /// Set the saved theme's wallpaper once, at startup.
        ///
        /// Nothing else did: apply-wallpaper.sh starts swaybg and caches the
        /// path, but swaybg dies with the session and only a theme *change* ever
        /// runs the script again — so a fresh login came up themed and
        /// wallpaperless. (The script's "read by the session's swaybg restore
        /// line on login" is paw v1's session script; paw2 has no equivalent.)
        ///
        /// Three things load asynchronously and any of them can be last, so this
        /// is called from all three and does nothing until they are all in:
        /// the saved slug, the theme list it indexes, and settings.json for a
        /// per-theme override.
        function restoreWallpaper() {
            if (wallpaperRestored
                || !Services.Settings.ready
                || root.currentSlug === ""
                || Object.keys(themeMap).length === 0)
                return;
            wallpaperRestored = true;
            applyWallpaper(root.currentSlug);
        }

        /// settings.json can finish loading after everything else.
        property Connections settingsConn: Connections {
            target: Services.Settings
            function onReadyChanged() { priv.restoreWallpaper(); }
        }

        // Read every themes/*.json in one POSIX sh pass, emitting each as
        // "\n===THEME:<slug>===\n<json>" so QML can split + parse it. Quickshell has no
        // reliable N-file reader for a non-visual singleton (Instantiator+FileView didn't
        // populate; Qt blocks file:// XHR), so this small shell read is the robust path.
        property Process listProc: Process {
            running: true
            command: ["sh", "-c",
                "for f in \"$1\"/*.json; do [ -e \"$f\" ] || continue; " +
                "printf '\\n===THEME:%s===\\n' \"$(basename \"$f\" .json)\"; cat \"$f\"; done",
                "sh", root.themesDir]
            stdout: StdioCollector {
                onStreamFinished: {
                    let map = {};
                    for (let c of this.text.split("\n===THEME:")) {
                        let end = c.indexOf("===\n");
                        if (end < 0) continue;
                        priv.ingestInto(map, c.slice(0, end), c.slice(end + 4));
                    }
                    priv.themeMap = map;
                    priv.rebuild();
                }
            }
        }

        // Parse one theme file into `map` (name + ordered swatches + wallpaper path).
        function ingestInto(map, slug, txt) {
            let name = slug, swatches = [], wp = "";
            try {
                let j = JSON.parse(txt || "{}");
                name = (j.meta && j.meta.name) || slug;
                let pal = j.palette || {};
                let order = ["bg", "red", "green", "yellow", "blue", "mauve", "cyan", "fg"];
                for (let k of order) {
                    let v = pal[k];
                    if (v) swatches.push(String(v).startsWith("#") ? String(v) : "#" + v);
                }
                wp = (j.wallpaper && j.wallpaper.path) || "";
            } catch (e) {
                console.warn("Theme: failed to parse", slug, e);
            }
            map[slug] = { slug: slug, name: name, swatches: swatches, wallpaper: wp };
        }

        // Hand the theme's wallpaper path to apply-wallpaper.sh (swaybg + cache).
        // The user's binding in settings.json wins over the theme JSON's own
        // path, so a shipped theme never overrides a choice the user made — and
        // themes still work when the bundled images aren't installed at all.
        // Relative paths resolve differently by source: a binding is relative to
        // the wallpaper directory the picker browses, a theme JSON's path is
        // relative to the paw2-shell config dir (where it was authored).
        function applyWallpaper(slug) {
            let entry = themeMap[slug];
            let override = Services.Settings.themeWallpapers[slug] || "";
            let p = override || (entry ? entry.wallpaper : "");
            if (!p) return;
            let path = p.charAt(0) === "/" ? p
                : (override ? Services.Settings.wallpaperDir + "/" + p
                            : root.themesDir + "/../" + p);
            wallpaperProc.command = ["bash", Services.Paths.script("apply-wallpaper.sh"), path];
            wallpaperProc.running = true;
        }

        function applyExternals(slug) {
            externalsProc.command = ["bash", Services.Paths.script("apply-theme-externals.sh"),
                slug,
                Services.Settings.themeKitty ? "1" : "0",
                Services.Settings.themeGtk ? "1" : "0"];
            externalsProc.running = true;
        }

        // Writes themes/<slug>.json, re-scans the list, then applies the saved slug.
        property FileView saveView: FileView {
            atomicWrites: true
            onSaved: { priv.listProc.running = true; root.apply(priv.pendingApplySlug); }
            onSaveFailed: error => console.warn("Theme: save failed:", error)
        }

        property Process writeProc: Process { running: false }
        property Process wallpaperProc: Process { running: false }
        property Process externalsProc: Process { running: false }

        // Track the persisted current slug so the UI can highlight it.
        property FileView currentView: FileView {
            path: Qt.resolvedUrl("file://" + root.statePath)
            watchChanges: true
            preload: true
            onFileChanged: this.reload()
            onTextChanged: {
                root.currentSlug = this.text().trim();
                priv.restoreWallpaper();
            }
            Component.onCompleted: {
                root.currentSlug = this.text().trim();
                priv.restoreWallpaper();
            }
        }
    }
}
