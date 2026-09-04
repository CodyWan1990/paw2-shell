import QtQuick
import "../services" as Services

/// Reusable section header — optional bold text + optional divider line.
Column {
    id: header
    width: parent?.width ?? 0
    spacing: 0

    property string text: ""
    property bool showDivider: true
    property int size: 16

    Rectangle {
        width: parent.width
        height: 1
        color: Services.Colors.color8
        opacity: 0.3
        visible: header.showDivider
    }

    StyledText {
        text: header.text
        color: Services.Colors.foreground
        font.pixelSize: header.size
        font.bold: true
        visible: header.text !== ""
        topPadding: header.showDivider && header.text !== "" ? 4 : 0
    }
}
