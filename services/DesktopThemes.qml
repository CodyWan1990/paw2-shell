pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io

/// What desktop themes are actually installed, so the settings UI can offer a
/// list instead of asking you to type a name exactly right.
///
/// One `sh` pass over the XDG theme roots, emitting labelled sections that this
/// splits apart — the same shape (and for the same reason) as Theme.qml's
/// listProc: Quickshell has no reliable directory walker for a non-visual
/// singleton, and a small shell read is the robust path.
///
/// Fonts are deliberately absent: `Settings.fontFamilies` already has them from
/// Qt, with no scanning needed.
QtObject {
    id: root

    /// GTK theme directory names — anything carrying a gtk-3.0 or gtk-4.0 dir.
    property var gtkThemes: []
    /// Icon themes: an index.theme, and not a cursor set.
    property var iconThemes: []
    /// Cursor themes: a cursors/ directory.
    property var cursorThemes: []
    /// Qt style names, from the installed style plugins plus Qt's built-ins.
    property var qtStyles: ["Fusion", "Windows"]

    /// Re-scan. Cheap, but nothing calls it on a timer: themes appear when a
    /// package is installed, which is not something to poll for.
    function refresh() { internal.scanProc.running = true; }

    readonly property QtObject internal: QtObject {
        id: internal

        /// `$1` is HOME. Every loop guards its own glob, so a missing root is
        /// the normal case rather than an error.
        readonly property string script: `
            h="$1"
            printf "===GTK===\\n"
            for d in /usr/share/themes "$h/.themes" "$h/.local/share/themes"; do
                [ -d "$d" ] || continue
                for t in "$d"/*/; do
                    [ -d "$t" ] || continue
                    if [ -d "$t/gtk-3.0" ] || [ -d "$t/gtk-4.0" ]; then
                        basename "$t"
                    fi
                done
            done
            printf "===ICONS===\\n"
            for d in /usr/share/icons "$h/.icons" "$h/.local/share/icons"; do
                [ -d "$d" ] || continue
                for t in "$d"/*/; do
                    [ -f "$t/index.theme" ] && [ ! -d "$t/cursors" ] && basename "$t"
                done
            done
            printf "===CURSORS===\\n"
            for d in /usr/share/icons "$h/.icons" "$h/.local/share/icons"; do
                [ -d "$d" ] || continue
                for t in "$d"/*/; do
                    [ -d "$t/cursors" ] && basename "$t"
                done
            done
            printf "===QT===\\n"
            for d in /usr/lib64/qt6/plugins/styles /usr/lib/qt6/plugins/styles; do
                [ -d "$d" ] || continue
                for f in "$d"/*.so; do
                    [ -e "$f" ] || continue
                    basename "$f" .so
                done
            done
        `

        /// Sort case-insensitively and drop duplicates — the same theme is
        /// often present in more than one root.
        function tidy(lines) {
            let seen = ({});
            let out = [];
            for (const line of lines) {
                const name = line.trim();
                if (name === "" || seen[name])
                    continue;
                seen[name] = true;
                out.push(name);
            }
            return out.sort((a, b) => a.toLowerCase().localeCompare(b.toLowerCase()));
        }

        /// `breeze6.so` is the plugin for the style called `Breeze`: drop the
        /// Qt major version the filename carries and capitalise what is left.
        function styleName(basename) {
            const bare = basename.replace(/[0-9]+$/, "");
            return bare.charAt(0).toUpperCase() + bare.slice(1);
        }

        function section(text, name) {
            const start = text.indexOf("===" + name + "===\n");
            if (start < 0)
                return [];
            const from = start + name.length + 7;
            const next = text.indexOf("\n===", from);
            return (next < 0 ? text.slice(from) : text.slice(from, next)).split("\n");
        }

        property Process scanProc: Process {
            running: true
            command: ["sh", "-c", internal.script, "sh", Quickshell.env("HOME")]
            stdout: StdioCollector {
                onStreamFinished: {
                    const text = this.text;
                    root.gtkThemes = internal.tidy(internal.section(text, "GTK"));
                    root.iconThemes = internal.tidy(internal.section(text, "ICONS"));
                    root.cursorThemes = internal.tidy(internal.section(text, "CURSORS"));
                    root.qtStyles = internal.tidy(
                        internal.section(text, "QT").filter(l => l.trim() !== "")
                            .map(internal.styleName)
                            .concat(["Fusion", "Windows"]));
                }
            }
        }
    }
}
