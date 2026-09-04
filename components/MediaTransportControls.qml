import QtQuick
import "../services" as Services

/// Previous/play/next controls shared by MPRIS surfaces.
Row {
    id: root

    property var player: null
    property color accentColor: Services.Colors.color1
    property int sideSize: 28
    property int playSize: 34
    property bool pointerCursor: true

    spacing: 28

    Text {
        text: "⏮"
        font.pixelSize: root.sideSize
        color: (root.player?.canGoPrevious ?? false)
            ? Services.Colors.foreground : Services.Colors.color8
        MouseArea {
            anchors.fill: parent
            cursorShape: root.pointerCursor ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: root.player?.previous()
        }
    }
    Text {
        textFormat: Text.PlainText
        text: root.player?.isPlaying ? "⏸" : "▶"
        font.pixelSize: root.playSize
        color: root.accentColor
        MouseArea {
            anchors.fill: parent
            cursorShape: root.pointerCursor ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: root.player?.togglePlaying()
        }
    }
    Text {
        text: "⏭"
        font.pixelSize: root.sideSize
        color: (root.player?.canGoNext ?? false)
            ? Services.Colors.foreground : Services.Colors.color8
        MouseArea {
            anchors.fill: parent
            cursorShape: root.pointerCursor ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: root.player?.next()
        }
    }
}
