import QtQuick
import "../services" as Services

/// Shared CPU/Memory detail-popup scaffold: header + two big headline values + a progress
/// bar + separator, then the label/value detail rows each monitor passes in `entries`.
Column {
    id: sp

    property string headerText: ""
    property string leftValue: ""     // rich text — caller controls color
    property string leftLabel: ""
    property string rightValue: ""     // rich text
    property string rightLabel: ""
    property bool rightVisible: true
    property real progress: 0          // 0–100 (ProgressBar.value)
    property color progressColor: Services.Colors.foreground

    /// Detail rows, forwarded to StatRows — see there for the format.
    property var entries: []

    // Anything that isn't a plain label/value row is appended below.
    default property alias extra: extraHolder.data

    width: parent.width
    spacing: 12

    PopupHeader {
        text: sp.headerText
        separatorOpacity: 0.3
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 16

        Column {
            anchors.verticalCenter: parent.verticalCenter
            StyledText {
                anchors.horizontalCenter: parent.horizontalCenter
                textFormat: Text.RichText
                color: Services.Colors.foreground   // rich-text <font color> overrides this
                font.pixelSize: 36
                font.bold: true
                text: sp.leftValue
            }
            StyledText {
                anchors.horizontalCenter: parent.horizontalCenter
                text: sp.leftLabel
                color: Services.Colors.color8
                font.pixelSize: 13
            }
        }

        Rectangle {
            width: 1; height: 50
            anchors.verticalCenter: parent.verticalCenter
            color: Services.Colors.color8; opacity: 0.3
            visible: sp.rightVisible
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            visible: sp.rightVisible
            StyledText {
                anchors.horizontalCenter: parent.horizontalCenter
                textFormat: Text.RichText
                color: Services.Colors.foreground
                font.pixelSize: 36
                font.bold: true
                text: sp.rightValue
            }
            StyledText {
                anchors.horizontalCenter: parent.horizontalCenter
                text: sp.rightLabel
                color: Services.Colors.color8
                font.pixelSize: 13
            }
        }
    }

    ProgressBar {
        value: sp.progress
        fillColor: sp.progressColor
    }

    Separator { }

    StatRows { entries: sp.entries }

    Column {
        id: extraHolder
        width: parent.width
    }
}
