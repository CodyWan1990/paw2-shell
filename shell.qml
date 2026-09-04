//@ pragma UseQApplication
import QtQuick
import Quickshell
import Quickshell.Io
import "panels" as Panels
import "services" as Services

/// Entry point — spawns a TopBar on every connected screen,
/// plus single-instance overlay panels (launcher, etc.).
ShellRoot {
    Variants {
        model: Quickshell.screens

        Panels.TopBar {
            required property var modelData
            screen: modelData
        }
    }

    Variants {
        model: Quickshell.screens

        Panels.Dock {
            required property var modelData
            screen: modelData
            outputName: modelData.name
        }
    }

    // Eager: it has to be live to catch a notification that arrives before
    // anything has opened it.
    Panels.NotificationToast {}

    // ─── Overlay panels ───
    //
    // None of these is on screen at login, and together they are most of the
    // shell's QML — so building them eagerly put ~4,800 lines between `qs`
    // starting and the bar drawing. `loading: true` keeps them off that path
    // while still warming them in the background once the bar is up, so the
    // first keypress does not pay for compiling one either. Reach them through
    // `panel()` below, never `.item` directly.
    LazyLoader { id: appLauncher; loading: true; Panels.AppLauncher {} }

    LazyLoader { id: clipboardPicker; loading: true; Panels.ClipboardPicker {} }

    LazyLoader { id: settingsWindow; loading: true; Panels.SettingsWindow {} }

    LazyLoader { id: powerMenu; loading: true; Panels.PowerMenu {} }

    LazyLoader { id: screenshotTool; loading: true; Panels.ScreenshotTool {} }

    LazyLoader { id: windowSwitcher; loading: true; Panels.WindowSwitcher {} }

    LazyLoader { id: welcome; loading: true; Panels.Welcome {} }

    LazyLoader { id: emojiPicker; loading: true; Panels.EmojiPicker {} }

    /// The panel inside a LazyLoader, built if it is not already.
    ///
    /// `loading: true` starts each one in the background but promises nothing
    /// about when it finishes, so a keypress in the first second could find
    /// `item` still null. Setting `active` completes the load synchronously,
    /// and is a no-op once loaded.
    function panel(loader) {
        loader.active = true;
        return loader.item;
    }

    // First run: show the setup screen once settings.json has been read (or
    // ruled absent). Binding on Settings.ready rather than Component.onCompleted
    // matters — the file loads asynchronously, and before it does firstRunDone
    // is still its compiled default, which would reopen this every login.
    Connections {
        target: Services.Settings
        function onReadyChanged() {
            if (Services.Settings.ready && !Services.Settings.firstRunDone)
                panel(welcome).open();
        }
    }

    // ─── Action bus ───
    Connections {
        target: Services.Actions
        function onLauncher() { panel(appLauncher).toggle(); }
        function onClipboard() { panel(clipboardPicker).toggle(); }
        function onPowermenu() { panel(powerMenu).toggle(); }
        function onWallpaper() { panel(appLauncher).toggleTo("wallpapers"); }
        function onThemes() { panel(appLauncher).toggleTo("themes"); }
        function onSettings() { panel(settingsWindow).toggle(); }
        // open(), not toggle(): a second Print while the panel is up must show the new
        // capture (open() re-fires opened() → cacheBuster bump), not close the panel.
        function onScreenshot() { panel(screenshotTool).open(); }
        // Nothing but paw2 can list windows, so on another compositor this
        // would only ever open an empty panel. Checked before `panel()`, so
        // elsewhere the switcher is never built at all.
        function onWindowswitcher() {
            if (Services.Paw.connected)
                panel(windowSwitcher).toggle();
        }
        function onWelcome() { panel(welcome).toggle(); }
        function onEmoji() { panel(emojiPicker).toggle(); }
    }

    // ─── IPC entry points ─── panel toggles, invoked from paw keybinds
    // (spawn qs -c paw2-shell ipc call shell <name>).
    IpcHandler {
        target: "shell"

        function launcher(): void { Services.Actions.launcher(); }
        function clipboard(): void { Services.Actions.clipboard(); }
        function powermenu(): void { Services.Actions.powermenu(); }
        function wallpaper(): void { Services.Actions.wallpaper(); }
        function themes(): void { Services.Actions.themes(); }
        function settings(): void { Services.Actions.settings(); }
        function screenshot(): void { Services.Actions.screenshot(); }
        function windowswitcher(): void { Services.Actions.windowswitcher(); }
        function welcome(): void { Services.Actions.welcome(); }
        function emoji(): void { Services.Actions.emoji(); }
    }
}
