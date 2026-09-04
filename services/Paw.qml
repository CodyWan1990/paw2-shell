pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io

/// paw2's control socket ($PAW2_SOCKET, JSON lines both ways).
///
/// The only file in the shell that knows the socket exists. Everything else
/// consumes the reactive properties below, which is what lets the rest of the
/// shell run unchanged on a compositor that has no socket at all - there,
/// `connected` simply stays false and the paw2-specific UI hides itself.
///
/// State only travels inwards and actions outwards. Settings are not written
/// through here: they go to paw2's `config.d/` as plain files (see PawConfig),
/// which the compositor already watches, so there is one path into the config
/// rather than two disagreeing about what is valid.
QtObject {
    id: root

    /// Per-output workspaces: [{ id, idx, name, output, isActive, isOccupied, isFocused }].
    /// `idx` is 1-based for display; the socket speaks 0-based indices.
    property var workspaces: []
    /// Title of the focused window ("" when none).
    property string focusedWindowTitle: ""
    /// All open windows: [{ id, title, appId, output, workspace, focused }]. Drives the
    /// window switcher.
    property var windows: []
    /// Connected outputs, in compositor order:
    /// [{ name, layout, modes: [{ width, height, refresh, current }] }].
    /// The mode list is paw2's own, and the only source of one: paw2 has no
    /// wlr-output-management, so `wlr-randr` cannot report modes either.
    property var outputs: []
    /// Layout names in cycle order, e.g. ["scrolling", "dwindle", ...].
    property var layouts: []
    /// The focused output's active layout, as a name and as an index into `layouts`.
    ///
    /// A per-monitor bar wants its *own* output's layout, not this one - see
    /// `layoutFor`. This pair stays for whoever genuinely means "the focused
    /// one".
    property string layoutName: ""
    readonly property int layoutIndex: Math.max(0, layouts.indexOf(layoutName))

    /// The layout active on `output`, falling back to the focused output's when
    /// the name is unknown (no snapshot yet, or running without paw2).
    function layoutFor(output) {
        const match = outputs.find(o => o.name === output);
        return match && match.layout ? match.layout : layoutName;
    }

    /// Index of `output`'s layout within `layouts`, for stepping through them.
    function layoutIndexFor(output) {
        return Math.max(0, layouts.indexOf(layoutFor(output)));
    }
    property bool connected: internal.socket.connected

    /// paw2's merged config - compiled defaults, then config.toml, then
    /// config.d/ - and the error from its last load. Sent on connect and after
    /// every reload, so nothing here duplicates paw2's defaults.
    property var config: ({})
    property var configErrors: []
    property bool configOk: true
    signal configResult(bool ok, var errors)

    /// ShellScreen of the focused output (null until the first snapshot).
    readonly property var focusedScreen: {
        let ws = workspaces.find(w => w.isFocused);
        if (!ws) return null;
        return Quickshell.screens.find(s => s.name === ws.output) ?? null;
    }

    /// `idx` is 1-based, matching what the bar displays. `output` is named so a
    /// click on one monitor's bar does not act on whichever monitor has focus.
    function focusWorkspace(output, idx) {
        send({ cmd: "focus-workspace", output: output, index: idx - 1 });
    }
    function focusWindow(id) { send({ cmd: "focus-window", id: id }); }
    /// `output` is optional and follows focus-workspace's rule: a bar is
    /// per-monitor, so a click can land on the layout widget of a monitor that
    /// does not have focus, and paw2 only ever changes the focused output's
    /// layout. Naming the output makes paw2 follow the click first.
    function setLayout(name, output) {
        send({ cmd: "set-layout", layout: name, output: output });
    }
    function cycleLayout(output) { send({ cmd: "set-layout", output: output }); }
    function quit() { send({ cmd: "quit" }); }

    function send(obj) {
        if (!internal.socket.connected) return;
        internal.socket.write(JSON.stringify(obj) + "\n");
    }

    readonly property QtObject internal: QtObject {
        id: internal

        function applySnapshot(s) {
            let ws = [], outs = [];
            for (let o of (s.outputs ?? [])) {
                // String(): `o` came out of JSON.parse as a QJSValue, and a
                // bare member of one will not assign to a QML string.
                outs.push({
                    name: o.name,
                    layout: String(o.layout ?? ""),
                    modes: o.modes ?? []
                });
                const occupied = o.occupied ?? [];
                for (let i = 0; i < occupied.length; i++) {
                    ws.push({
                        id: o.name + ":" + i,
                        idx: i + 1,
                        name: String(i + 1),
                        output: o.name,
                        isActive: i === o.active,
                        isOccupied: occupied[i] === true,
                        isFocused: i === o.active && o.focused === true
                    });
                }
                if (o.focused === true)
                    root.layoutName = String(o.layout ?? "");
            }
            // paw2 coalesces its snapshots to one per event-loop iteration, but
            // a snapshot still arrives whenever anything at all changes - and
            // most changes leave these lists identical. They are `var`
            // properties compared by reference, so assigning an equal array
            // still fires the change signal, rebuilding WorkspaceBar's Repeater
            // and re-running WindowSwitcher's filter for nothing.
            assignIfChanged("workspaces", ws);
            assignIfChanged("outputs", outs);
            assignIfChanged("layouts", s.layouts ?? []);
            const windows = (s.windows ?? []).map(w => ({
                id: w.id,
                title: w.title ?? "",
                appId: w.app_id ?? "",
                output: w.output ?? "",
                workspace: (w.workspace ?? 0) + 1,
                focused: w.focused === true
            }));
            assignIfChanged("windows", windows);
            // A string, so it already only signals on a real change.
            const focused = (s.windows ?? []).find(w => w.focused === true);
            root.focusedWindowTitle = focused ? (focused.title ?? "") : "";
        }

        function assignIfChanged(prop, next) {
            if (JSON.stringify(next) !== JSON.stringify(root[prop]))
                root[prop] = next;
        }

        function handle(msg) {
            switch (msg.type) {
            case "snapshot":
                applySnapshot(msg);
                break;
            case "config":
                root.config = msg.values ?? {};
                root.configOk = msg.ok === true;
                root.configErrors = msg.errors ?? [];
                root.configResult(root.configOk, root.configErrors);
                break;
            default:
                break;
            }
        }

        property Socket socket: Socket {
            path: Quickshell.env("PAW2_SOCKET") ?? ""
            // Only attempt a connection when there is a socket to connect to;
            // without paw2 this stays false and the shell runs standalone.
            connected: (Quickshell.env("PAW2_SOCKET") ?? "") !== ""
            parser: SplitParser {
                onRead: line => {
                    let text = line.trim();
                    if (text.length === 0) return;
                    try {
                        internal.handle(JSON.parse(text));
                    } catch (e) {
                        console.warn("Paw: failed to parse message:", e, "-", text);
                    }
                }
            }
            onConnectedChanged: {
                if (!connected && (Quickshell.env("PAW2_SOCKET") ?? "") !== "") {
                    console.warn("Paw: socket disconnected; reconnecting in 1s");
                    internal.reconnect.start();
                }
            }
        }

        property Timer reconnect: Timer {
            interval: 1000
            onTriggered: internal.socket.connected = true
        }
    }
}
