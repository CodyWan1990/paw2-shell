import QtQuick
import "../services" as Services

/// Selection/hover surface for popup and overlay result rows.
Rectangle {
    id: root

    default property alias content: contentItem.data
    property bool selected: false
    property color accentColor: Services.Colors.color5
    property color hoverColor: Services.Colors.withAlpha(accentColor, 0.3)
    property color selectedColor: accentColor
    property int clickRightMargin: 0
    readonly property bool hovered: mouse.containsMouse

    signal clicked()
    signal entered()
    signal pointerMoved()

    color: selected ? selectedColor : (hovered ? hoverColor : "transparent")

    Item {
        id: contentItem
        anchors.fill: parent
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        anchors.rightMargin: root.clickRightMargin
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onEntered: root.entered()
        onPositionChanged: root.pointerMoved()
        onClicked: root.clicked()
    }
}
