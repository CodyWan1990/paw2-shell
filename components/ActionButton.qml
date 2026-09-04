import QtQuick
import QtQuick.Layouts
import "../services" as Services

/// Big icon+label action button (fills width; accent fill when hovered/selected).
Rectangle {
    id: btn

    property string icon: ""
    property string label: ""
    property color activeColor: Services.Colors.accent
    property bool selected: false
    signal clicked()
    signal hovered()

    readonly property bool active: ma.containsMouse || selected
    Layout.fillWidth: true
    height: 50
    color: active ? activeColor : Services.Colors.color0
    border.color: active ? activeColor : Services.Colors.color8
    border.width: 1

    Column {
        anchors.centerIn: parent
        spacing: 2
        StyledText {
            text: btn.icon
            visible: btn.icon !== ""
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: 18
            color: btn.active ? Services.Colors.background : Services.Colors.foreground
        }
        StyledText {
            text: btn.label
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: 14
            color: btn.active ? Services.Colors.background : Services.Colors.foreground
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: btn.clicked()
        onContainsMouseChanged: if (containsMouse) btn.hovered()
    }
}
