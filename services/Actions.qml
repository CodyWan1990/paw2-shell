pragma Singleton

import QtQuick

/// In-process action bus — bar items and the IpcHandler (driven by paw keybinds) emit these; shell.qml wires them to panel toggles.
QtObject {
    signal launcher()
    signal clipboard()
    signal windowswitcher()
    signal powermenu()
    signal wallpaper()
    signal themes()
    signal settings()
    signal screenshot()
    signal welcome()
    signal emoji()
}
