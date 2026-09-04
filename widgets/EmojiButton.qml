import QtQuick
import "../services" as Services
import "../components" as Components

/// Emoji picker bar widget — click opens the emoji picker overlay.
Components.BarWidget {
    icon: "emoji"
    defaultAccent: Services.Colors.color3
    clickable: true
    // Through the action bus, not a direct panel reference: the panel is a
    // single instance in shell.qml and this widget exists once per screen.
    onClicked: Services.Actions.emoji()
}
