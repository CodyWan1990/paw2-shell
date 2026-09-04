pragma Singleton

import QtQuick
import Quickshell

/// Central location for paw2-shell's on-disk paths (was repeated inline across many files).
QtObject {
    readonly property string base: Quickshell.env("HOME") + "/.config/quickshell/paw2-shell"
    readonly property string scriptsDir: base + "/scripts"
    readonly property string themesDir: base + "/themes"
    readonly property string settingsPath: base + "/settings.json"
    readonly property string weatherConf: base + "/weather.conf"
    readonly property string statePath: Quickshell.env("HOME") + "/.local/state/paw2-shell/current"
    /// paw2's own config directory (compositor-owned; XDG_CONFIG_HOME aware).
    readonly property string pawDir: (Quickshell.env("XDG_CONFIG_HOME") || (Quickshell.env("HOME") + "/.config")) + "/paw2"
    /// paw2 merges every *.toml here over its config.toml, in filename order.
    /// config.toml itself stays the user's: nothing in the shell writes it.
    readonly property string overlayDir: pawDir + "/config.d"
    /// Generated wholesale by the settings UI. Machine-owned - hand edits here
    /// are overwritten on the next change made through the GUI.
    readonly property string settingsOverlay: overlayDir + "/shell.toml"
    /// Hand-typed, and separate so the generator above can never clobber it.
    readonly property string keybindsOverlay: overlayDir + "/keybinds.toml"

    function script(name) { return scriptsDir + "/" + name }
}
