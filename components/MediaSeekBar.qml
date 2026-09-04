import QtQuick
import QtQuick.Layouts
import "../services" as Services

/// Polling MPRIS seek/progress row with shared time formatting.
Column {
    id: root

    property var player: null
    property color accentColor: Services.Colors.accent
    property color trackColor: Services.Colors.color0
    property int trackHeight: 8
    property bool seekEnabled: true
    property int tick: 0

    spacing: 2
    visible: player?.lengthSupported ?? false

    Timer {
        interval: 1000
        running: root.visible && (root.player?.isPlaying ?? false)
        repeat: true
        onTriggered: root.tick++
    }

    Rectangle {
        width: parent.width
        height: root.trackHeight
        radius: height / 2
        color: root.trackColor

        Rectangle {
            width: {
                root.tick;
                let length = root.player?.length ?? 0;
                let position = root.player?.position ?? 0;
                return length > 0 ? parent.width * Math.min(1, position / length) : 0;
            }
            height: parent.height
            radius: parent.radius
            color: root.accentColor
        }

        MouseArea {
            anchors.fill: parent
            enabled: root.seekEnabled && (root.player?.canSeek ?? false)
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: event => {
                let length = root.player?.length ?? 0;
                if (length <= 0) return;
                let target = length * event.x / width;
                root.player.seek(target - (root.player?.position ?? 0));
                root.tick++;
            }
        }
    }

    RowLayout {
        width: parent.width
        StyledText {
            text: {
                root.tick;
                return Services.Media.formatTime(root.player?.position ?? 0);
            }
            color: Services.Colors.color8
            font.pixelSize: 11
        }
        Item { Layout.fillWidth: true }
        StyledText {
            text: Services.Media.formatTime(root.player?.length ?? 0)
            color: Services.Colors.color8
            font.pixelSize: 11
        }
    }
}
