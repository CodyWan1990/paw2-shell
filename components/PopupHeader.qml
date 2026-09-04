import QtQuick
import QtQuick.Layouts
import "../services" as Services

/// Popup title row with optional close button and bottom separator.
Column {
    id: root
    width: parent ? parent.width : 0
    spacing: parent ? parent.spacing : 12

    property string text: ""
    property bool showClose: false
    property bool showSeparator: true
    property real separatorOpacity: 1.0

    signal closeClicked()

    RowLayout {
        width: parent.width

        StyledText {
            text: root.text
            color: Services.Colors.foreground
            font.pixelSize: 17
            font.bold: true
            Layout.fillWidth: true
        }

        Text {
            text: "\u2715"
            color: Services.Colors.color8
            font.pixelSize: 17
            visible: root.showClose
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.closeClicked()
            }
        }
    }

    Separator {
        visible: root.showSeparator
        opacity: root.separatorOpacity
    }
}
