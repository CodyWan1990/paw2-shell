//@ pragma UseQApplication
import QtQuick
import Quickshell
import "services" as Services

/// Regression test for Paw.applySnapshot's change-signal churn.
///
/// paw2 sends a snapshot whenever anything at all changes, and a window title
/// carrying a spinner changes several times a second. The reactive properties
/// are `var`, so assigning an equal array still emits a change signal and
/// rebuilds every consumer's Repeater. This asserts a redundant snapshot is
/// silent.
///
/// Run with scripts/check-snapshot.sh, which stages this as the shell.qml of a
/// throwaway config and unsets PAW2_SOCKET — otherwise the singleton connects to
/// the running compositor and real snapshots land mid-assertion.
ShellRoot {
    id: root

    property int failures: 0
    function check(cond, what) {
        if (!cond) { console.warn("FAIL: " + what); failures++; }
        else console.warn("  ok: " + what);
    }

    property int wsSignals: 0
    property int winSignals: 0
    property int outSignals: 0

    property Connections watch: Connections {
        target: Services.Paw
        function onWorkspacesChanged() { root.wsSignals++; }
        function onWindowsChanged() { root.winSignals++; }
        function onOutputsChanged() { root.outSignals++; }
    }

    // Two outputs, nine workspaces each — the shape paw2 actually sends.
    function snapshot(title, active) {
        function output(name, focused, act) {
            return {
                name: name,
                focused: focused,
                active: act,
                occupied: [true, true, false, false, false, false, false, false, false],
                layout: focused ? "scrolling" : "dwindle",
                modes: [{ width: 2560, height: 1440, refresh: 144.0, current: true }]
            };
        }
        return {
            outputs: [output("DP-1", true, active), output("DP-2", false, 0)],
            layouts: ["scrolling", "dwindle", "master_stack", "monocle"],
            windows: [{ id: 1, title: title, app_id: "foot",
                        output: "DP-1", workspace: 0, focused: true }]
        };
    }

    Component.onCompleted: {
        let P = Services.Paw;

        console.warn("— a first snapshot populates");
        P.internal.applySnapshot(snapshot("one", 0));
        check(P.workspaces.length === 18, "18 workspaces from 2 outputs x 9");
        check(P.focusedWindowTitle === "one", "title lands");
        check(P.layoutName === "scrolling", "the focused output's layout lands");
        check(P.layoutIndex === 0, "...and resolves to an index into layouts");

        console.warn("— every output reports its own layout, not the focused one's");
        // The bug this covers: a per-monitor bar read the focused output's
        // layout, so both bars showed the same thing however each monitor was
        // actually tiled. DP-2 is deliberately on a different layout here.
        check(P.layoutFor("DP-1") === "scrolling", "the focused output's own layout");
        check(P.layoutFor("DP-2") === "dwindle", "...and the unfocused one's differs");
        check(P.layoutIndexFor("DP-2") === 1, "...resolving to its own index");
        check(P.layoutFor("nope") === P.layoutName,
            "an output paw2 has not reported falls back to the focused layout");
        let ws0 = root.wsSignals, win0 = root.winSignals, out0 = root.outSignals;
        check(ws0 > 0, "...and it did signal, since the content changed");

        console.warn("— a spinner tick: same state, only the title differs");
        // This is the case that mattered: paw2 sends the whole snapshot on a
        // title change, so workspaces/outputs/layouts are byte-identical.
        P.internal.applySnapshot(snapshot("two", 0));
        check(P.focusedWindowTitle === "two", "the title still updates");
        check(root.wsSignals === ws0,
            "workspaces did NOT re-signal (was " + ws0 + ", now "
            + root.wsSignals + ") — this is the bug that rebuilt the bar 10x/s");
        check(root.outSignals === out0, "outputs did not re-signal either");
        check(root.winSignals > win0,
            "windows DID re-signal, because the window's title genuinely changed");

        console.warn("— an identical snapshot is completely silent");
        let ws1 = root.wsSignals, win1 = root.winSignals, out1 = root.outSignals;
        P.internal.applySnapshot(snapshot("two", 0));
        check(root.wsSignals === ws1 && root.winSignals === win1
                && root.outSignals === out1,
            "a byte-identical snapshot emits nothing at all");

        console.warn("— a real change still gets through");
        P.internal.applySnapshot(snapshot("two", 3));
        check(root.wsSignals > ws1,
            "switching workspace re-signals — the guard must not swallow "
            + "genuine updates");
        check(P.workspaces[3].isActive === true,
            "...and the new selection is reflected (workspace 4 active)");
        check(P.workspaces[3].isFocused === true,
            "...on the focused output, that is also the focused workspace");
        check(P.workspaces[0].isOccupied === true,
            "occupancy survives the round trip");

        console.warn("— outputs carry the modes paw2 reports");
        check(P.outputs.length === 2 && P.outputs[0].modes.length === 1,
            "a mode list reaches the Monitors page");

        console.warn("— windows are exposed 1-based, matching the bar");
        check(P.windows[0].workspace === 1,
            "workspace 0 on the wire is workspace 1 in the UI");
        check(P.windows[0].appId === "foot", "app_id is camelCased for QML");
        check(P.windows[0].focused === true, "focused state reaches window consumers");

        console.warn("— degenerate payloads do not throw");
        P.internal.applySnapshot({});
        check(P.workspaces.length === 0, "an empty snapshot clears cleanly");
        check(P.focusedWindowTitle === "", "...including the title");

        console.warn(failures === 0
            ? "\nRESULT: all snapshot-churn tests passed"
            : "\nRESULT: " + failures + " test(s) FAILED");
        Qt.quit();
    }
}
