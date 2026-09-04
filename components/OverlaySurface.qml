import QtQuick
import "../services" as Services

/// Shared centered surface chrome for full-screen overlays.
Rectangle {
    id: root

    default property alias content: contentItem.data
    property color accentColor: Services.Colors.accent

    radius: Services.Ui.radiusLg
    color: Services.Colors.background
    border.color: accentColor
    border.width: 1
    clip: true

    // Consume clicks so they do not reach the overlay backdrop.
    MouseArea { anchors.fill: parent }

    Item {
        id: contentItem
        anchors.fill: parent
    }
}
