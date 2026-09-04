pragma Singleton

import QtQuick
import Quickshell.Io
import "." as Services

/// paw2's runtime config, read from the compositor and written as a drop-in.
///
/// Reads come from paw2 over the socket: it sends the merged document - its
/// compiled defaults, then the user's config.toml, then config.d/ - on connect
/// and after every reload. So there is exactly one copy of the defaults, and it
/// is paw2's.
///
/// Writes never touch config.toml. They regenerate one drop-in file,
/// `config.d/shell.toml`, which paw2 applies over the user's config and reloads
/// within half a second. That keeps a hand-edited config.toml - comments,
/// ordering and all - exactly as its author left it, and keeps paw2
/// authoritative for what is valid: a rejected value comes back as an error
/// here rather than being enforced twice.
///
/// paw2 gives a drop-in every top-level table it names *whole*, so this file
/// owns each table it writes into. Two consequences it has to honour:
///
/// - `claim` seeds the rest of a table on the first write into it, or the keys
///   with no control here would drop to paw2's compiled defaults.
/// - Once a table is claimed, config.toml's version of it is inert. Deleting
///   `config.d/` is what hands it back.
///
/// Ownership is also what makes a coupled rule keepable - `focus_ring_width`
/// cannot exceed `gaps`, and nothing could hold that while one writer had each
/// side. CompositorTab holds it now; see its `maxPath`.
///
/// `[bindings]` is deliberately never written from here. It belongs to
/// `config.d/keybinds.toml`, and since `shell.toml` sorts after it, claiming
/// that table would take the whole section off the keybinds editor.
QtObject {
    id: root

    /// Live values, compositor-order. Pending edits are overlaid so sliders
    /// stay responsive while the write + reload round trip completes.
    readonly property var values: internal.overlay(Services.Paw.config, internal.pending)

    /// True once paw2 has sent its config at least once.
    readonly property bool ready: Object.keys(Services.Paw.config).length > 0
    /// Errors from paw2's last load, e.g. ["appearance.gaps must be at most 256"].
    readonly property var errors: Services.Paw.configErrors
    readonly property bool ok: Services.Paw.configOk

    /// Read a dotted path, e.g. get("appearance.corner_radius", 8).
    function get(path, fallback) {
        let node = values;
        for (const part of path.split(".")) {
            if (node === undefined || node === null || typeof node !== "object")
                return fallback;
            node = node[part];
        }
        return node === undefined ? fallback : node;
    }

    /// Write a dotted path. Debounced: the drop-in is rewritten once the user
    /// stops moving the slider.
    function set(path, value) {
        if (JSON.stringify(get(path, undefined)) === JSON.stringify(value))
            return;
        // A fresh object, not a mutated one: assigning the same reference back
        // to a var property raises no change notification, so `values` would
        // keep serving its cached copy.
        let p = Object.assign({}, internal.pending);
        p[path] = value;
        internal.pending = p;
        internal.saveTimer.restart();
    }

    /// Set one key on one monitor's `[[monitors]]` entry, creating the entry
    /// if paw2 has none for that connector yet.
    ///
    /// Monitors are an array, and merging an array replaces it wholesale, so
    /// the drop-in has to carry every monitor once it carries any. They are
    /// re-seeded from paw2's effective config on each write, which already
    /// includes whatever config.toml declared, so nothing is lost — but from
    /// then on a hand-edit to config.toml's `[[monitors]]` is shadowed by this
    /// file. Deleting the drop-in hands control back.
    function setMonitor(connector, key, value) {
        let monitors = JSON.parse(JSON.stringify(internal.monitors()));
        let entry = monitors.find(m => m.connector === connector);
        if (!entry) {
            entry = { connector: connector };
            monitors.push(entry);
        }
        if (JSON.stringify(entry[key]) === JSON.stringify(value))
            return;
        entry[key] = value;
        internal.pendingMonitors = monitors;
        internal.ownsMonitors = true;
        internal.saveTimer.restart();
    }

    /// Force the write now (e.g. when a settings panel closes).
    function flush() {
        internal.saveTimer.stop();
        internal.save();
    }

    readonly property QtObject internal: QtObject {
        id: internal

        /// Dotted-path edits not yet confirmed by a config message from paw2.
        property var pending: ({})
        /// The whole `[[monitors]]` array, when an edit is in flight.
        property var pendingMonitors: null
        /// Whether the drop-in carries `[[monitors]]` at all. Once it does it
        /// always must, or the next write would silently drop the entries.
        property bool ownsMonitors: false

        /// The effective monitor list, with any in-flight edit on top.
        function monitors() {
            return pendingMonitors ?? (Services.Paw.config.monitors ?? []);
        }
        /// Everything the shell has ever set, dotted path to value. The drop-in
        /// is rewritten whole every time, so it has to carry the earlier edits
        /// too; it is seeded from the file on startup.
        property var owned: ({})
        property bool loaded: false
        /// An edit arrived before the file finished loading; write it as soon
        /// as it has.
        property bool saveWhenLoaded: false

        /// Deep copy of base with each dotted path in edits applied on top.
        function overlay(base, edits) {
            let out = JSON.parse(JSON.stringify(base ?? {}));
            for (const path in edits) {
                const parts = path.split(".");
                let node = out;
                for (let i = 0; i < parts.length - 1; i++) {
                    if (typeof node[parts[i]] !== "object" || node[parts[i]] === null)
                        node[parts[i]] = {};
                    node = node[parts[i]];
                }
                node[parts[parts.length - 1]] = edits[path];
            }
            return out;
        }

        /// A value as TOML. Only the shapes paw2's config actually uses:
        /// booleans, numbers, strings, and flat arrays of those.
        function toml(v) {
            if (typeof v === "boolean")
                return v ? "true" : "false";
            // No special case for integer-valued floats: serde reads a TOML
            // integer into an f64 field happily, so `opacity = 1` is fine.
            if (typeof v === "number")
                return String(v);
            if (Array.isArray(v))
                return "[" + v.map(toml).join(", ") + "]";
            return JSON.stringify(String(v));
        }

        /// A TOML key: bare when it can be, quoted when it cannot. Binding
        /// names like `Super+Return` fall in the second group.
        function tomlKey(key) {
            return /^[A-Za-z0-9_-]+$/.test(key) ? key : JSON.stringify(key);
        }

        /// Render `owned` as a TOML document, grouped into sections.
        ///
        /// Every path in paw2's config is `section.key`, so one level of
        /// grouping covers it; a deeper path would need a dotted-key line,
        /// which nothing currently produces.
        function render(paths) {
            let sections = ({});
            for (const path of Object.keys(paths).sort()) {
                const cut = path.lastIndexOf(".");
                if (cut < 0)
                    continue;
                const section = path.slice(0, cut);
                const key = path.slice(cut + 1);
                if (!sections[section])
                    sections[section] = [];
                sections[section].push(tomlKey(key) + " = " + toml(paths[path]));
            }
            let out = "# Generated by paw2-shell. Every value here is set from the\n"
                + "# settings UI and this file is rewritten whole on each change.\n"
                + "# Hand edits belong in config.toml, which the shell never touches.\n";
            for (const section of Object.keys(sections).sort()) {
                const header = section.split(".").map(tomlKey).join(".");
                out += "\n[" + header + "]\n" + sections[section].join("\n") + "\n";
            }
            return out;
        }

        /// Flatten a config table into dotted paths, one entry per leaf. A leaf
        /// is anything that is not a plain object: scalars, and the flat arrays
        /// paw2's config uses.
        function flatten(prefix, table, out) {
            for (const key in table) {
                const value = table[key];
                const path = prefix + "." + key;
                if (value && typeof value === "object" && !Array.isArray(value))
                    flatten(path, value, out);
                else
                    out[path] = value;
            }
            return out;
        }

        /// Whether the drop-in already supplies the table `top`.
        function claimed(top) {
            for (const path in owned)
                if (path.indexOf(top + ".") === 0)
                    return true;
            return false;
        }

        /// Everything paw2 currently has in the tables `paths` is about to write
        /// into, for the ones the drop-in does not carry yet.
        ///
        /// paw2 gives a drop-in every table it names *whole*, so the first write
        /// into one has to bring the rest of it along. Without this, setting
        /// `appearance.gaps` would quietly reset `blur_radius` and everything
        /// else in `[appearance]` to a compiled default. It is the same reason
        /// `setMonitor` re-seeds the whole `[[monitors]]` array.
        function claim(paths) {
            let seed = ({});
            for (const path of Object.keys(paths)) {
                const top = path.slice(0, path.indexOf("."));
                if (top === "" || claimed(top))
                    continue;
                const table = root.get(top, null);
                if (table && typeof table === "object" && !Array.isArray(table))
                    flatten(top, table, seed);
            }
            return seed;
        }

        function save() {
            if (Object.keys(pending).length === 0 && pendingMonitors === null)
                return;
            // The drop-in is read asynchronously, and a setting changed before
            // that finishes must not be dropped: hold the write until `adopt`
            // has recovered what earlier sessions put in the file, or it would
            // be rewritten with only this one edit in it.
            if (!loaded) {
                saveWhenLoaded = true;
                return;
            }
            saveWhenLoaded = false;
            // `claim` never overlaps `owned` - it only seeds tables `owned` has
            // nothing in - and `pending` wins over the seed it just read.
            owned = Object.assign({}, owned, claim(pending), pending);
            awaitingResult = true;
            file.setText(render(owned) + (ownsMonitors ? renderMonitors(monitors()) : ""));
        }

        /// `[[monitors]]` blocks. Only the keys paw2 defines, so a stray key
        /// picked up from its config cannot be written back into a form its
        /// `deny_unknown_fields` would then reject.
        readonly property var monitorKeys: [
            "connector", "focus_at_startup", "resolution", "refresh",
            "position", "scale", "transform", "vrr"
        ]

        function renderMonitors(list) {
            let out = "";
            for (const monitor of list) {
                if (!monitor.connector)
                    continue;
                out += "\n[[monitors]]\n";
                for (const key of monitorKeys)
                    if (monitor[key] !== undefined)
                        out += key + " = " + toml(monitor[key]) + "\n";
            }
            return out;
        }

        property Timer saveTimer: Timer {
            interval: 400
            onTriggered: internal.save()
        }

        /// The drop-in itself. Read once at startup to recover what the shell
        /// set in earlier sessions, then only written.
        property FileView file: FileView {
            path: Services.Paths.settingsOverlay
            preload: true
            atomicWrites: true

            onLoaded: internal.adopt(this.text())
            // No file yet is the first-run case, not an error: adopt nothing,
            // which still marks the reader done and releases a held write.
            onLoadFailed: internal.adopt("")
        }

        /// Recover the dotted paths already in the drop-in, so a later write
        /// does not drop settings made in an earlier session. Deliberately a
        /// small reader for the subset `render` emits, not a TOML parser.
        function adopt(text) {
            let section = "";
            let found = ({});
            for (let line of (text ?? "").split("\n")) {
                line = line.trim();
                if (line.length === 0 || line.startsWith("#"))
                    continue;
                if (line.startsWith("[[")) {
                    // Array-of-tables; handled separately, and its keys are
                    // not dotted paths.
                    section = "";
                    continue;
                }
                if (line.startsWith("[") && line.endsWith("]")) {
                    section = line.slice(1, -1).trim();
                    continue;
                }
                const cut = line.indexOf("=");
                if (cut < 0 || section === "")
                    continue;
                const key = line.slice(0, cut).trim();
                try {
                    // Every shape `toml` writes is also valid JSON, except that
                    // TOML has no other spelling for these scalars either.
                    found[section + "." + key] = JSON.parse(line.slice(cut + 1).trim());
                } catch (e) {
                    // A value the shell did not write. Leaving it out means the
                    // next save drops it, which is correct: this file is ours.
                    console.warn("PawConfig: ignoring unreadable line:", line);
                }
            }
            internal.owned = found;
            // What is on disk is the baseline a refused write rewinds to. Left
            // at its empty default, the first refusal would blank the drop-in
            // instead of undoing one value.
            internal.lastGood = found;
            // Not parsed back: the entries are re-seeded from paw2's effective
            // config, which has already merged this file. All that is needed
            // from here is whether this file is the one carrying them.
            internal.ownsMonitors = internal.ownsMonitors
                || (text ?? "").indexOf("[[monitors]]") >= 0;
            internal.loaded = true;
            if (internal.saveWhenLoaded)
                internal.save();
        }

        /// `owned` as paw2 last accepted it, and whether a write of ours is
        /// still waiting for its verdict.
        property var lastGood: ({})
        property bool awaitingResult: false

        /// paw2 has confirmed a reload; its values are authoritative again.
        property Connections pawConn: Connections {
            target: Services.Paw
            function onConfigResult(ok, errors) {
                internal.pending = ({});
                internal.pendingMonitors = null;
                if (ok) {
                    internal.lastGood = internal.owned;
                    internal.awaitingResult = false;
                    return;
                }
                console.warn("PawConfig: paw2 reported", errors.length,
                             "config error(s):", errors.join("; "));
                // Only undo a refusal of our own write. A hand-edit that breaks
                // config.toml reports the same way, and rewinding the drop-in
                // then would lose settings without fixing anything.
                if (!internal.awaitingResult)
                    return;
                internal.awaitingResult = false;
                // Put the file back to what paw2 last accepted. Leaving the
                // refused value in `owned` would re-emit it on every later
                // write, so one bad value would silently turn the whole
                // settings UI into a no-op - which is exactly what it used to do.
                internal.owned = internal.lastGood;
                internal.file.setText(internal.render(internal.owned)
                    + (internal.ownsMonitors
                        ? internal.renderMonitors(internal.monitors()) : ""));
            }
        }
    }
}
