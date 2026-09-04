//@ pragma UseQApplication
import QtQuick
import Quickshell
import "services" as Services

/// Regression test for dock matching, ordering and window cycling.
ShellRoot {
    property int failures: 0
    function check(condition, what) {
        if (!condition) { console.warn("FAIL: " + what); failures++; }
        else console.warn("  ok: " + what);
    }

    readonly property var catalog: [
        { id: "firefox.desktop", name: "Firefox", icon: "firefox",
          startupClass: "Firefox", desktop: ({}) },
        { id: "org.kde.kate.desktop", name: "Kate", icon: "kate",
          startupClass: "kate", desktop: ({}) }
    ]

    Component.onCompleted: {
        const A = Services.Applications;
        check(A.entryForAppId(catalog, "FIREFOX").id === "firefox.desktop",
            "startup class matching is case-insensitive");
        check(A.entryForAppId(catalog, "org.kde.kate").name === "Kate",
            "desktop id matching ignores the .desktop suffix");

        const windows = [
            { id: 1, appId: "Firefox", output: "DP-1", focused: true },
            { id: 2, appId: "firefox", output: "DP-1", focused: false },
            { id: 3, appId: "kate", output: "DP-2", focused: false },
            { id: 4, appId: "mystery", output: "DP-1", focused: false }
        ];
        const items = A.buildDockItems(catalog, windows,
            ["org.kde.kate.desktop", "missing.desktop"], "DP-1");
        check(items.map(item => item.name).join(",") === "Kate,missing,Firefox,mystery",
            "pins lead in saved order, followed by local running apps");
        check(items[0].windows.length === 0, "another monitor's window stays off this dock");
        check(items[1].entry === null, "an uninstalled pin remains removable");
        check(items[2].windows.length === 2, "windows from one app share an icon");
        const pinsOnly = A.buildDockItems(catalog, windows,
            ["firefox.desktop"], "DP-1", false);
        check(pinsOnly.length === 1 && pinsOnly[0].windows.length === 2,
            "hiding unpinned apps keeps windows grouped under pins");
        check(A.nextWindow(items[2].windows).id === 2,
            "a click on the focused app cycles to its next window");
        check(A.nextWindow([{ id: 9, focused: false }]).id === 9,
            "an unfocused app selects its first window");

        const S = Services.Settings;
        S.dockPinnedApps = ["one.desktop", "Two.desktop", "three.desktop"];
        S.moveDockApp("TWO.DESKTOP", 0);
        check(S.dockPinnedApps.join(",") === "Two.desktop,one.desktop,three.desktop",
            "pin reordering is case-insensitive and preserves the saved ids");
        S.moveDockApp("one.desktop", 2);
        check(S.dockPinnedApps.join(",") === "Two.desktop,one.desktop,three.desktop",
            "dropping a pin on its current boundary is a no-op");
        check(S.isDockAppPinned("TWO.DESKTOP"),
            "pin lookup is case-insensitive");
        S.toggleDockApp("two.desktop");
        check(S.dockPinnedApps.join(",") === "one.desktop,three.desktop",
            "pin toggle removes an existing app");
        S.toggleDockApp("Two.desktop");
        check(S.dockPinnedApps.join(",") === "one.desktop,three.desktop,Two.desktop",
            "pin toggle appends a newly pinned app using its original id");

        console.warn(failures === 0
            ? "\nRESULT: all dock-model tests passed"
            : "\nRESULT: " + failures + " test(s) FAILED");
        Qt.quit();
    }
}
