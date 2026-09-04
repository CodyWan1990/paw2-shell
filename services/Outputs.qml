pragma Singleton

import QtQuick
import Quickshell.Io
import "." as Services

/// Monitors — enumerated and reconfigured, with two backends behind one API.
///
/// Under paw2, the monitor list comes from the control socket and a change is
/// written to paw2's config as a `[[monitors]]` entry. paw2 does not implement
/// wlr-output-management-v1, so `wlr-randr` cannot see or change anything.
/// Mode, scale and transform changes apply live through paw2's config watcher;
/// position still needs a restart.
///
/// Anywhere else, this is `wlr-randr`, which applies immediately. It is an
/// optional dependency: `available` is false when it is not installed, and
/// callers should say so rather than show empty dropdowns.
QtObject {
    id: root

    /// [{ name, description, enabled, x, y, scale, transform, modes: [...] }],
    /// each mode { width, height, refresh, current, preferred, label }.
    readonly property var list: paw ? internal.pawList : internal.list
    /// True when paw2 is driving this, rather than wlr-randr.
    readonly property bool paw: Services.Paw.connected
    /// False when there is no backend at all: no paw2 and no wlr-randr.
    readonly property bool available: paw || internal.available
    /// False when any supported applied change needs a compositor restart.
    readonly property bool liveChanges: !paw
    readonly property bool loading: internal.loading

    signal applied(string name, bool ok)

    function refresh() {
        // Under paw2 the list is pushed, not polled.
        if (root.paw || internal.loading)
            return;
        internal.loading = true;
        internal.listProc.running = true;
    }

    /// Look up one output by name, or null.
    function find(name) {
        for (const o of list)
            if (o.name === name)
                return o;
        return null;
    }

    /// "2560x1440@143.998Hz" as wlr-randr both prints and accepts it.
    function modeLabel(mode) {
        return mode.width + "x" + mode.height
            + (mode.refresh ? "@" + mode.refresh.toFixed(3) + "Hz" : "");
    }

    /// Apply a change to one output. opts: { mode, position, scale, transform,
    /// enabled } — every key optional, only what is passed is touched. Emits
    /// applied(name, ok) and re-reads the real state after.
    ///
    /// Under paw2 this writes `[[monitors]]` and reports success immediately:
    /// the config is validated on reload, and any complaint arrives through
    /// PawConfig.errors rather than from here.
    function apply(name, opts) {
        if (root.paw) {
            internal.applyThroughConfig(name, opts);
            root.applied(name, true);
            return;
        }
        if (!internal.available)
            return;
        let args = ["wlr-randr", "--output", name];
        if (opts.enabled === false) {
            args.push("--off");
        } else {
            if (opts.enabled === true)
                args.push("--on");
            if (opts.mode)
                args.push("--mode", opts.mode);
            if (opts.position)
                args.push("--pos", opts.position);
            if (opts.scale !== undefined)
                args.push("--scale", String(opts.scale));
            if (opts.transform)
                args.push("--transform", opts.transform);
        }
        internal.applying = name;
        // Success is reported on stdout rather than through the exit code:
        // Process.exited carries a QProcess enum the QML tooling cannot resolve,
        // and every other service here already reads its answer from stdout.
        internal.applyProc.command = ["sh", "-c", '"$@" && echo ok', "sh"].concat(args);
        internal.applyProc.running = true;
    }

    readonly property QtObject internal: QtObject {
        id: internal

        property var list: []
        property bool available: true

        /// paw2's outputs in the same shape wlr-randr's give, so every caller
        /// and both dropdowns stay backend-agnostic. Position, scale and
        /// transform come from the config rather than the socket. Scale and
        /// transform apply live; position remains a restart-only setting.
        readonly property var pawList: Services.Paw.outputs.map(o => {
            const configured = (Services.PawConfig.get("monitors", []) ?? [])
                .find(m => m.connector === o.name) ?? ({});
            return {
                name: o.name,
                description: o.name,
                enabled: true,
                x: (configured.position ?? [0, 0])[0],
                y: (configured.position ?? [0, 0])[1],
                scale: configured.scale ?? 1,
                transform: configured.transform ?? "normal",
                modes: (o.modes ?? []).map(m => ({
                    width: m.width, height: m.height,
                    refresh: m.refresh || 0,
                    current: !!m.current, preferred: !!m.current,
                    label: root.modeLabel(m)
                }))
            };
        })

        /// Translate one apply() into `[[monitors]]` keys. Only the options
        /// paw2's config actually has. Disabling an output is not expressible
        /// there at all.
        function applyThroughConfig(name, opts) {
            if (opts.mode) {
                // "2560x1440@143.998Hz" — the one string form apply() takes.
                const parts = opts.mode.match(/^(\d+)x(\d+)(?:@([\d.]+)Hz)?$/);
                if (parts) {
                    Services.PawConfig.setMonitor(name, "resolution",
                        [parseInt(parts[1]), parseInt(parts[2])]);
                    if (parts[3])
                        Services.PawConfig.setMonitor(name, "refresh", parseFloat(parts[3]));
                } else {
                    console.warn("Outputs: cannot parse mode", opts.mode);
                }
            }
            if (opts.position) {
                const xy = opts.position.split(",");
                Services.PawConfig.setMonitor(name, "position",
                    [parseInt(xy[0]), parseInt(xy[1])]);
            }
            if (opts.transform)
                Services.PawConfig.setMonitor(name, "transform", opts.transform);
            if (opts.scale !== undefined)
                Services.PawConfig.setMonitor(name, "scale", opts.scale);
            if (opts.enabled === false)
                console.warn("Outputs: paw2 cannot disable an output from config");
        }
        property bool loading: false
        property string applying: ""

        // The sentinel doubles as the "wlr-randr is not installed" answer, so
        // availability needs no second probe.
        property Process listProc: Process {
            command: ["sh", "-c",
                "command -v wlr-randr >/dev/null 2>&1 || { echo missing; exit 0; }; "
                + "wlr-randr --json"]
            stdout: StdioCollector {
                onStreamFinished: {
                    internal.loading = false;
                    internal.parse(this.text);
                }
            }
        }

        property Process applyProc: Process {
            stdout: StdioCollector {
                onStreamFinished: {
                    root.applied(internal.applying, this.text.trim() === "ok");
                    internal.applying = "";
                    root.refresh();
                }
            }
            stderr: StdioCollector {
                onStreamFinished: {
                    if (this.text.trim().length > 0)
                        console.warn("Outputs: wlr-randr:", this.text.trim());
                }
            }
        }

        function parse(text) {
            let raw;
            const s = text.trim();

            if (s === "missing") {
                internal.available = false;
                internal.list = [];
                return;
            }
            try {
                raw = JSON.parse(s || "[]");
            } catch (e) {
                console.warn("Outputs: cannot parse wlr-randr output:", e);
                return;
            }
            if (!Array.isArray(raw))
                return;
            internal.available = true;
            internal.list = raw.map(o => ({
                name: o.name || "",
                // What the user recognises the monitor by: "Dell 27\"" beats
                // "DP-1", but the name is all some outputs report.
                description: [o.make, o.model].filter(s => s && s !== "Unknown").join(" ")
                    || o.name || "",
                enabled: o.enabled !== false,
                x: (o.position && o.position.x) || 0,
                y: (o.position && o.position.y) || 0,
                scale: o.scale || 1,
                transform: o.transform || "normal",
                modes: (o.modes || []).map(m => ({
                    width: m.width, height: m.height,
                    refresh: m.refresh || 0,
                    current: !!m.current, preferred: !!m.preferred,
                    label: root.modeLabel(m)
                }))
            }));
        }

        // Only meaningful for the wlr-randr backend; refresh() is a no-op
        // under paw2, which pushes its list instead.
        Component.onCompleted: root.refresh()
    }
}
