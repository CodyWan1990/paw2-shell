//@ pragma UseQApplication
import QtQuick
import Quickshell
import "services" as Services

/// Regression test for restoring the wallpaper at login.
///
/// swaybg dies with the session, and only a theme *change* ever ran
/// apply-wallpaper.sh — so a fresh login came up with the theme applied and no
/// wallpaper at all. Theme.restoreWallpaper fixes that, but it depends on three
/// things that load asynchronously in no fixed order: the saved slug, the theme
/// list it indexes, and settings.json for a per-theme override. It fires once,
/// when the last of them lands.
///
/// Run with scripts/check-wallpaper.sh, which stages a throwaway HOME holding a
/// saved slug and one theme. That theme's wallpaper is a path that does not
/// exist **on purpose**: apply-wallpaper.sh checks the file before it touches
/// swaybg, so the script exits without killing the real session's wallpaper.
/// The assertion is on the command Theme built, which is what proves the right
/// image was resolved.
ShellRoot {
    id: root

    property int failures: 0
    function check(cond, what) {
        if (!cond) { console.warn("FAIL: " + what); failures++; }
        else console.warn("  ok: " + what);
    }

    readonly property string missing: "/nonexistent/paw2-test-wallpaper.png"

    property Timer settle: Timer {
        interval: 300
        running: true
        repeat: true
        property int ticks: 0
        onTriggered: {
            const T = Services.Theme;
            // Wait for the three loads, or give up and report what is missing.
            if (!T.internal.wallpaperRestored && ++ticks < 20)
                return;
            running = false;
            root.run();
        }
    }

    function run() {
        const T = Services.Theme;

        console.warn("— the three inputs the restore waits on");
        check(Services.Settings.ready, "settings.json was read or ruled absent");
        check(T.currentSlug === "testtheme", "the saved slug loaded");
        check(T.themes.length === 1, "the theme list loaded");

        console.warn("— the restore fires once, with the saved theme's wallpaper");
        check(T.internal.wallpaperRestored, "it fired");
        const command = T.internal.wallpaperProc.command;
        check(command.length === 3, "a three-part command was built");
        check(String(command[1]).indexOf("apply-wallpaper.sh") >= 0,
            "...calling apply-wallpaper.sh");
        check(String(command[2]) === root.missing,
            "...with the wallpaper the saved theme names");

        console.warn("— and it does not fire twice");
        // A second trigger is what an extra reload or a settings write would
        // look like; the one-shot flag has to swallow it, or every such event
        // would restart swaybg under the user.
        T.internal.wallpaperProc.command = ["sentinel"];
        T.internal.restoreWallpaper();
        check(String(T.internal.wallpaperProc.command[0]) === "sentinel",
            "a second call is a no-op");

        console.warn(failures === 0
            ? "\nRESULT: all wallpaper-restore tests passed"
            : "\nRESULT: " + failures + " test(s) FAILED");
        Qt.quit();
    }
}
